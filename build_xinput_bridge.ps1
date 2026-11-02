param(
    [switch]$Deploy,
    [ValidateRange(1, 120)][int]$TestSeconds = 10,
    [string]$Compiler = ''
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$source = Join-Path $root 'xinput_winmm_bridge.c'
$exports = Join-Path $root 'xinput_winmm_bridge.def'
$test = Join-Path $root 'test_xinput_bridge.py'
$target = Join-Path $root 'WuwaTappo'
$systemDll = Join-Path $env:WINDIR 'System32\xinput1_4.dll'
if (-not ((Test-Path -LiteralPath $source) -and (Test-Path -LiteralPath $exports) -and (Test-Path -LiteralPath $test))) {
    throw 'Bridge source, export definition, or test script is missing.'
}
if (-not (Test-Path -LiteralPath $systemDll)) {
    throw "Missing native XInput DLL: $systemDll"
}
if (-not (Test-Path -LiteralPath (Join-Path $target 'WuwaTappo.exe'))) {
    throw "WuwaTappo destination is missing: $target"
}

if ($Compiler) {
    $compilerCommand = Get-Command $Compiler -ErrorAction Stop
} else {
    $compilerCommand = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $compilerCommand) { $compilerCommand = Get-Command gcc.exe -ErrorAction SilentlyContinue }
}
if (-not $compilerCommand) {
    throw 'No native C compiler on PATH. Install a trusted x64 MSVC Build Tools or MinGW-w64 toolchain, open its developer shell, then rerun this script.'
}
$compilerPath = $compilerCommand.Source
$compilerName = [IO.Path]::GetFileName($compilerPath).ToLowerInvariant()
if ($compilerName -notin @('cl.exe', 'gcc.exe')) {
    throw 'Compiler must be cl.exe or gcc.exe for this build script.'
}
$python = Get-Command python.exe -ErrorAction Stop
$stage = Join-Path $env:TEMP ("wuwatappo-xinput-{0}-{1}" -f (Get-Date -Format 'yyyyMMdd-HHmmss'), $PID)
New-Item -ItemType Directory -Path $stage -ErrorAction Stop | Out-Null
$dll = Join-Path $stage 'xinput1_4.dll'
$sidecar = Join-Path $stage 'xinput1_4_system.dll'
Copy-Item -LiteralPath $systemDll -Destination $sidecar -ErrorAction Stop
Write-Host "Building in $stage"
if ($compilerName -eq 'cl.exe') {
    & $compilerPath /nologo /O2 /LD /W4 "/Fo$stage\bridge.obj" "/Fe$dll" $source /link "/DEF:$exports"
} else {
    & $compilerPath -shared -O2 -std=c11 -Wall -Wextra -o $dll $source $exports
}
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $dll)) {
    throw "Native DLL build failed (exit $LASTEXITCODE); no desktop pet files were changed."
}
Write-Host 'Press a DualSense D-pad direction and face button during the test.'
& $python.Source -B $test $dll $TestSeconds
if ($LASTEXITCODE -ne 0) {
    throw "DLL test failed (exit $LASTEXITCODE); no desktop pet files were changed."
}
if (-not $Deploy) {
    Write-Host "Staged DLL passed. To deploy, rerun with -Deploy; staged files: $stage"
    exit 0
}
if (Get-Process -Name WuwaTappo -ErrorAction SilentlyContinue) {
    throw 'Desktop pet is running. Exit it normally and rerun -Deploy; no desktop pet files were changed.'
}
$existing = Join-Path $target 'xinput1_4.dll'
$existingSidecar = Join-Path $target 'xinput1_4_system.dll'
$backupSuffix = (Get-Date -Format 'yyyyMMdd-HHmmss')
foreach ($item in @($existing, $existingSidecar)) {
    if (Test-Path -LiteralPath $item) {
        $backup = "$item.bak-$backupSuffix"
        Copy-Item -LiteralPath $item -Destination $backup -ErrorAction Stop
        Write-Host "Backed up $item to $backup"
    }
}
Copy-Item -LiteralPath $sidecar -Destination $existingSidecar -ErrorAction Stop
Copy-Item -LiteralPath $dll -Destination $existing -ErrorAction Stop
Write-Host "Deployed $existing and $existingSidecar. Restart WuwaTappo and verify its loaded module path."
Write-Host 'To remove the bridge, exit WuwaTappo then remove the two deployed DLLs; restore any .bak files if present.'
