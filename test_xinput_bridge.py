"""Smoke-test a staged x64 XInput proxy using only Python's standard library.

Usage: python -B test_xinput_bridge.py PATH_TO_STAGED_DLL [seconds]
Press the DualSense D-pad and one face button during the test interval.
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


def check_pe_machine(path):
    with path.open("rb") as f:
        header = f.read(64)
        if header[:2] != b"MZ":
            raise ValueError("not a PE DLL")
        f.seek(struct.unpack_from("<I", header, 0x3C)[0] + 4)
        machine = struct.unpack("<H", f.read(2))[0]
        if machine != 0x8664:
            raise ValueError(f"expected AMD64 PE, got 0x{machine:04x}")


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit("Usage: python -B test_xinput_bridge.py DLL_PATH [seconds]")
    path = pathlib.Path(sys.argv[1]).resolve(strict=True)
    seconds = float(sys.argv[2]) if len(sys.argv) == 3 else 10
    if seconds <= 0:
        raise SystemExit("seconds must be positive")
    check_pe_machine(path)
    if not (path.parent / "xinput1_4_system.dll").is_file():
        raise SystemExit("missing ordinal-forwarder sidecar xinput1_4_system.dll")
    if ct.sizeof(State) != 16 or ct.sizeof(Capabilities) != 20:
        raise SystemExit("unexpected Windows ctypes ABI")

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
    capacity = Capabilities()
    rc = caps(0, 0, ct.byref(capacity))
    if rc != 0:
        raise SystemExit(f"XInputGetCapabilities(0) returned {rc}, expected 0")
    print(f"x64 exports OK; capabilities: type={capacity.type} buttons=0x{capacity.gamepad.buttons:04x}", flush=True)
    print("Press D-pad and a face button now; testing for state changes...", flush=True)
    initial = State()
    rc = get(0, ct.byref(initial))
    if rc != 0:
        raise SystemExit(f"XInputGetState(0) returned {rc}, expected 0")
    print(f"initial: packet={initial.packet} buttons=0x{initial.gamepad.buttons:04x}", flush=True)
    last = None
    saw_dpad = False
    saw_face = False
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        state = State()
        rc = get(0, ct.byref(state))
        if rc:
            raise SystemExit(f"XInputGetState(0) unexpectedly returned {rc}")
        current = (state.packet, state.gamepad.buttons,
                   state.gamepad.left_trigger, state.gamepad.right_trigger)
        if current != last:
            print("packet=%d buttons=0x%04x LT=%d RT=%d" % current, flush=True)
        saw_dpad |= bool(state.gamepad.buttons & 0x000F)
        saw_face |= bool(state.gamepad.buttons & 0xF000)
        last = current
        time.sleep(0.02)
    print(f"RESULT dpad_seen={saw_dpad} face_button_seen={saw_face}", flush=True)
    if not saw_dpad or not saw_face:
        raise SystemExit("Need both D-pad and face-button presses to certify and deploy this DLL")


if __name__ == "__main__":
    main()
