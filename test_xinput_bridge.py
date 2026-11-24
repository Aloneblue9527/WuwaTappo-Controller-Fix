r"""Verify standalone zero-CRT x64 xinput1_4.dll (buttons, triggers, sticks).

Usage: python -B test_xinput_bridge.py .\xinput1_4.dll [seconds]
"""

import ctypes as ct
import pathlib
import struct
import sys
import time
from ctypes import wintypes as wt


class Gamepad(ct.Structure):
    _fields_ = [
        ("buttons", wt.WORD), ("left_trigger", ct.c_ubyte),
        ("right_trigger", ct.c_ubyte), ("lx", ct.c_short),
        ("ly", ct.c_short), ("rx", ct.c_short), ("ry", ct.c_short),
    ]


class State(ct.Structure):
    _fields_ = [("packet", wt.DWORD), ("gamepad", Gamepad)]


class Vibration(ct.Structure):
    _fields_ = [("left", wt.WORD), ("right", wt.WORD)]


class Capabilities(ct.Structure):
    _fields_ = [
        ("type", ct.c_ubyte), ("subtype", ct.c_ubyte),
        ("flags", wt.WORD), ("gamepad", Gamepad),
        ("vibration", Vibration),
    ]


def inspect_pe(path: pathlib.Path):
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("not a PE DLL")
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_off:pe_off + 4] != b"PE\0\0":
        raise ValueError("invalid PE header")
    machine = struct.unpack_from("<H", data, pe_off + 4)[0]
    if machine != 0x8664:
        raise ValueError(f"expected AMD64 PE, got 0x{machine:04x}")

    num_sections = struct.unpack_from("<H", data, pe_off + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe_off + 20)[0]
    opt_off = pe_off + 24
    import_rva = struct.unpack_from("<I", data, opt_off + 120)[0]

    sections = []
    sec_off = opt_off + opt_size
    for i in range(num_sections):
        vsize, vaddr, raw_size, raw_ptr = struct.unpack_from("<4I", data, sec_off + i * 40 + 8)
        sections.append((vaddr, max(vsize, raw_size), raw_ptr))

    def rva_to_off(rva):
        for vaddr, span, raw_ptr in sections:
            if vaddr <= rva < vaddr + span:
                return raw_ptr + (rva - vaddr)
        raise ValueError(f"RVA 0x{rva:x} out of range")

    imports = []
    if import_rva:
        off = rva_to_off(import_rva)
        while True:
            desc = struct.unpack_from("<5I", data, off)
            if not any(desc):
                break
            name_off = rva_to_off(desc[3])
            end = data.index(b"\0", name_off)
            imports.append(data[name_off:end].decode("ascii", errors="replace"))
            off += 20
    return len(data), imports


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit("Usage: python -B test_xinput_bridge.py DLL_PATH [seconds]")
    path = pathlib.Path(sys.argv[1]).resolve(strict=True)
    seconds = float(sys.argv[2]) if len(sys.argv) == 3 else 10

    size, imports = inspect_pe(path)
    print(f"PE OK: {size} bytes | Static imports: {imports}", flush=True)
    bad = [d for d in imports if d.lower() != "kernel32.dll"]
    if bad:
        raise SystemExit(f"NOT zero-CRT! Unexpected imports: {bad}")

    dll = ct.WinDLL(str(path))
    k32 = ct.WinDLL("kernel32.dll")
    proc = k32.GetProcAddress
    proc.argtypes = [wt.HMODULE, ct.c_void_p]
    proc.restype = ct.c_void_p
    for ordinal in (1, 2, 3, 4, 5, 7, 8, 10, 100, 101, 102, 103, 104, 108, 109):
        if not proc(dll._handle, ct.c_void_p(ordinal)):
            raise SystemExit(f"missing XInput export ordinal {ordinal}")

    get = dll.XInputGetState
    get.argtypes = [wt.DWORD, ct.POINTER(State)]
    get.restype = wt.DWORD
    caps = dll.XInputGetCapabilities
    caps.argtypes = [wt.DWORD, wt.DWORD, ct.POINTER(Capabilities)]
    caps.restype = wt.DWORD

    cap = Capabilities()
    rc = caps(0, 0, ct.byref(cap))
    if rc != 0:
        raise SystemExit(f"XInputGetCapabilities(0) returned {rc} (is controller connected?)")
    print(f"All 15 exports OK | Capabilities: buttons=0x{cap.gamepad.buttons:04x} LX=0x{cap.gamepad.lx & 0xFFFF:04x}", flush=True)
    print("Press D-pad, face buttons, triggers, and move both sticks...", flush=True)

    last = None
    saw_dpad = False
    saw_face = False
    saw_stick = False
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        st = State()
        rc = get(0, ct.byref(st))
        if rc:
            raise SystemExit(f"XInputGetState(0) returned {rc}")
        g = st.gamepad
        cur = (g.buttons, g.left_trigger, g.right_trigger, g.lx, g.ly, g.rx, g.ry)
        if cur != last:
            print(f"pkt={st.packet:4d} btn=0x{g.buttons:04x} LT={g.left_trigger:3d} RT={g.right_trigger:3d} "
                  f"L=({g.lx:6d},{g.ly:6d}) R=({g.rx:6d},{g.ry:6d})", flush=True)
        saw_dpad |= bool(g.buttons & 0x000F)
        saw_face |= bool(g.buttons & 0xF000)
        saw_stick |= any(abs(v) > 10000 for v in (g.lx, g.ly, g.rx, g.ry))
        last = cur
        time.sleep(0.03)
    print(f"RESULT dpad={saw_dpad} face={saw_face} stick={saw_stick}", flush=True)


if __name__ == "__main__":
    main()
