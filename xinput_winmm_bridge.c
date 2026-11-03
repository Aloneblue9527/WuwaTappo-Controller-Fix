/* Local-only XInput 1.4 proxy for WinMM-compatible controllers (V2 Multi-Slot).
 * Scans all WinMM slots (0..15) and merges with native XInput to prevent
 * slot-shift failures and virtual controller short-circuiting.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <wchar.h>

#define DEVICE_NOT_CONNECTED 1167UL
#define JOY_RETURNALL_LOCAL 0x00ffUL
#define MAX_WINMM_SLOTS 16U
#define XINPUT_GAMEPAD_DPAD_UP 0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN 0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT 0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT 0x0008
#define XINPUT_GAMEPAD_START 0x0010
#define XINPUT_GAMEPAD_BACK 0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB 0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB 0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER 0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A 0x1000
#define XINPUT_GAMEPAD_B 0x2000
#define XINPUT_GAMEPAD_X 0x4000
#define XINPUT_GAMEPAD_Y 0x8000

typedef struct {
    DWORD dwSize, dwFlags, dwXpos, dwYpos, dwZpos, dwRpos, dwUpos, dwVpos;
    DWORD dwButtons, dwButtonNumber, dwPOV, dwReserved1, dwReserved2;
} JOYINFOEX_LOCAL;

typedef struct {
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} GAMEPAD_LOCAL;

typedef struct { DWORD dwPacketNumber; GAMEPAD_LOCAL Gamepad; } STATE_LOCAL;
typedef struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } VIBRATION_LOCAL;
typedef struct {
    BYTE Type, SubType;
    WORD Flags;
    GAMEPAD_LOCAL Gamepad;
    VIBRATION_LOCAL Vibration;
} CAPS_LOCAL;

typedef DWORD (WINAPI *GetStateFn)(DWORD, STATE_LOCAL *);
typedef DWORD (WINAPI *SetStateFn)(DWORD, VIBRATION_LOCAL *);
typedef DWORD (WINAPI *GetCapsFn)(DWORD, DWORD, CAPS_LOCAL *);
typedef UINT (WINAPI *GetJoyFn)(UINT, JOYINFOEX_LOCAL *);

DWORD WINAPI XInputGetState(DWORD index, STATE_LOCAL *state);

static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
static SRWLOCK g_lock = SRWLOCK_INIT;
static GetStateFn g_real_get;
static SetStateFn g_real_set;
static GetCapsFn g_real_caps;
static GetJoyFn g_joy_get;
static GAMEPAD_LOCAL g_previous;
static DWORD g_packet;
static BOOL g_was_connected;

static HMODULE load_system(const wchar_t *dll) {
    wchar_t path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    size_t length = 0;
    while (dll[length]) ++length;
    if (!n || (size_t)n + length + 2 > MAX_PATH) return NULL;
    path[n++] = L'\\';
    memcpy(path + n, dll, (length + 1) * sizeof(wchar_t));
    return LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

static HMODULE load_original_sidecar(void) {
    HMODULE self = NULL;
    wchar_t path[MAX_PATH];
    wchar_t *slash;
    static const wchar_t filename[] = L"xinput1_4_system.dll";
    size_t length = sizeof(filename) / sizeof(filename[0]);
    DWORD n;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             (LPCWSTR)(void *)&XInputGetState, &self)) return NULL;
    n = GetModuleFileNameW(self, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return NULL;
    slash = wcsrchr(path, L'\\');
    if (!slash || (size_t)(slash - path) + length >= MAX_PATH) return NULL;
    memcpy(slash + 1, filename, sizeof(filename));
    return LoadLibraryW(path);
}

static BOOL CALLBACK initialize(PINIT_ONCE once, PVOID param, PVOID *context) {
    HMODULE xinput = load_system(L"xinput1_4.dll");
    HMODULE winmm = load_system(L"winmm.dll");
    (void)once; (void)param; (void)context;
    if (xinput && GetProcAddress(xinput, "XInputGetState") == (FARPROC)(void *)&XInputGetState)
        xinput = load_original_sidecar();
    if (xinput) {
        g_real_get = (GetStateFn)GetProcAddress(xinput, "XInputGetState");
        g_real_set = (SetStateFn)GetProcAddress(xinput, "XInputSetState");
        g_real_caps = (GetCapsFn)GetProcAddress(xinput, "XInputGetCapabilities");
    }
    if (winmm) g_joy_get = (GetJoyFn)GetProcAddress(winmm, "joyGetPosEx");
    return TRUE;
}

static GAMEPAD_LOCAL convert(const JOYINFOEX_LOCAL *joy) {
    static const WORD button_map[14] = {
        XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B,
        XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_LEFT_SHOULDER,
        XINPUT_GAMEPAD_RIGHT_SHOULDER,
        XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_BACK,
        XINPUT_GAMEPAD_RIGHT_SHOULDER | XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
        XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_START,
        XINPUT_GAMEPAD_LEFT_THUMB | XINPUT_GAMEPAD_RIGHT_THUMB
    };
    GAMEPAD_LOCAL out = {0};
    unsigned i;
    for (i = 0; i < 14; ++i)
        if (joy->dwButtons & (1UL << i)) out.wButtons |= button_map[i];
    if (joy->dwButtons & (1UL << 6)) out.bLeftTrigger = 255;
    if (joy->dwButtons & (1UL << 7)) out.bRightTrigger = 255;
    if (joy->dwPOV != 65535UL && joy->dwPOV < 36000UL) {
        DWORD angle = joy->dwPOV;
        if (angle >= 31500UL || angle <= 4500UL) out.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        if (angle >= 4500UL && angle <= 13500UL) out.wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        if (angle >= 13500UL && angle <= 22500UL) out.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (angle >= 22500UL && angle <= 31500UL) out.wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
    }
    return out;
}

/* Scans all WinMM slots 0..15 and merges button states from any connected controller */
static BOOL read_all_joysticks(GAMEPAD_LOCAL *merged) {
    UINT slot;
    BOOL any_connected = FALSE;
    memset(merged, 0, sizeof(*merged));
    if (!g_joy_get) return FALSE;
    for (slot = 0; slot < MAX_WINMM_SLOTS; ++slot) {
        JOYINFOEX_LOCAL joy;
        memset(&joy, 0, sizeof(joy));
        joy.dwSize = sizeof(joy);
        joy.dwFlags = JOY_RETURNALL_LOCAL;
        if (g_joy_get(slot, &joy) == 0) {
            GAMEPAD_LOCAL cur = convert(&joy);
            any_connected = TRUE;
            merged->wButtons |= cur.wButtons;
            if (cur.bLeftTrigger > merged->bLeftTrigger)
                merged->bLeftTrigger = cur.bLeftTrigger;
            if (cur.bRightTrigger > merged->bRightTrigger)
                merged->bRightTrigger = cur.bRightTrigger;
        }
    }
    return any_connected;
}

DWORD WINAPI XInputGetState(DWORD index, STATE_LOCAL *state) {
    DWORD real_rc;
    STATE_LOCAL real_state;
    GAMEPAD_LOCAL winmm_pad;
    BOOL winmm_ok;
    if (!state) return ERROR_BAD_ARGUMENTS;
    InitOnceExecuteOnce(&g_once, initialize, NULL, NULL);
    if (index != 0) {
        return g_real_get ? g_real_get(index, state) : DEVICE_NOT_CONNECTED;
    }
    memset(&real_state, 0, sizeof(real_state));
    real_rc = g_real_get ? g_real_get(0, &real_state) : DEVICE_NOT_CONNECTED;
    winmm_ok = read_all_joysticks(&winmm_pad);

    if (real_rc != ERROR_SUCCESS && !winmm_ok) {
        AcquireSRWLockExclusive(&g_lock);
        g_was_connected = FALSE;
        ReleaseSRWLockExclusive(&g_lock);
        return real_rc;
    }

    if (real_rc == ERROR_SUCCESS) {
        winmm_pad.wButtons |= real_state.Gamepad.wButtons;
        if (real_state.Gamepad.bLeftTrigger > winmm_pad.bLeftTrigger)
            winmm_pad.bLeftTrigger = real_state.Gamepad.bLeftTrigger;
        if (real_state.Gamepad.bRightTrigger > winmm_pad.bRightTrigger)
            winmm_pad.bRightTrigger = real_state.Gamepad.bRightTrigger;
    }

    AcquireSRWLockExclusive(&g_lock);
    if (!g_was_connected || memcmp(&winmm_pad, &g_previous, sizeof(winmm_pad)) != 0) {
        ++g_packet;
        g_previous = winmm_pad;
        g_was_connected = TRUE;
    }
    state->dwPacketNumber = g_packet;
    ReleaseSRWLockExclusive(&g_lock);
    state->Gamepad = winmm_pad;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputGetCapabilities(DWORD index, DWORD flags, CAPS_LOCAL *caps) {
    DWORD result;
    GAMEPAD_LOCAL dummy;
    if (!caps) return ERROR_BAD_ARGUMENTS;
    InitOnceExecuteOnce(&g_once, initialize, NULL, NULL);
    result = g_real_caps ? g_real_caps(index, flags, caps) : DEVICE_NOT_CONNECTED;
    if (result == ERROR_SUCCESS || index != 0 || result != DEVICE_NOT_CONNECTED) return result;
    if (flags != 0 && flags != 1) return ERROR_BAD_ARGUMENTS;
    if (!read_all_joysticks(&dummy)) return DEVICE_NOT_CONNECTED;
    memset(caps, 0, sizeof(*caps));
    caps->Type = 1; caps->SubType = 1;
    caps->Gamepad.wButtons = 0xf3ff;
    caps->Gamepad.bLeftTrigger = caps->Gamepad.bRightTrigger = 255;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputSetState(DWORD index, VIBRATION_LOCAL *vibration) {
    DWORD result;
    GAMEPAD_LOCAL dummy;
    if (!vibration) return ERROR_BAD_ARGUMENTS;
    InitOnceExecuteOnce(&g_once, initialize, NULL, NULL);
    result = g_real_set ? g_real_set(index, vibration) : DEVICE_NOT_CONNECTED;
    if (result == ERROR_SUCCESS || index != 0 || result != DEVICE_NOT_CONNECTED) return result;
    return read_all_joysticks(&dummy) ? ERROR_SUCCESS : DEVICE_NOT_CONNECTED;
}
