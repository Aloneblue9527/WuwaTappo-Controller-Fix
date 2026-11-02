"""Poll a WinMM gamepad and optionally bridge button edges to F24.

Test first: python controller_bridge.py --seconds 10
Send:       python controller_bridge.py --any-input --send --seconds 0
Stop:       Ctrl+C

This does not modify WuwaTappo or configure its hotkey. F24 must be bound
inside the pet before --send can be expected to count as a clap.
"""

import argparse
import ctypes as ct
import sys
import time
from ctypes import wintypes as wt


class JOYINFOEX(ct.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD), ("dwFlags", wt.DWORD),
        ("dwXpos", wt.DWORD), ("dwYpos", wt.DWORD),
        ("dwZpos", wt.DWORD), ("dwRpos", wt.DWORD),
        ("dwUpos", wt.DWORD), ("dwVpos", wt.DWORD),
        ("dwButtons", wt.DWORD), ("dwButtonNumber", wt.DWORD),
        ("dwPOV", wt.DWORD), ("dwReserved1", wt.DWORD),
        ("dwReserved2", wt.DWORD),
    ]


class KEYBDINPUT(ct.Structure):
    _fields_ = [
        ("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
        ("time", wt.DWORD), ("dwExtraInfo", ct.c_size_t),
    ]


class MOUSEINPUT(ct.Structure):
    _fields_ = [
        ("dx", wt.LONG), ("dy", wt.LONG), ("mouseData", wt.DWORD),
        ("dwFlags", wt.DWORD), ("time", wt.DWORD),
        ("dwExtraInfo", ct.c_size_t),
    ]


class HARDWAREINPUT(ct.Structure):
    _fields_ = [("uMsg", wt.DWORD), ("wParamL", wt.WORD), ("wParamH", wt.WORD)]


class INPUTUNION(ct.Union):
    _fields_ = [("ki", KEYBDINPUT), ("mi", MOUSEINPUT), ("hi", HARDWAREINPUT)]


class INPUT(ct.Structure):
    _fields_ = [("type", wt.DWORD), ("u", INPUTUNION)]


POV = {"up": 0, "right": 9000, "down": 18000, "left": 27000}
POV_CENTER = 65535
VK_F24 = 0x87


def input_edge(current_buttons, current_pov, old_buttons, old_pov):
    """Return true for a newly pressed button bit or a new POV direction."""
    if old_buttons is None:
        return False
    new_buttons = current_buttons & ~old_buttons
    new_direction = current_pov != POV_CENTER and current_pov != old_pov
    return bool(new_buttons or new_direction)
KEYEVENTF_KEYUP = 0x0002
JOY_RETURNALL = 0x00FF


def main():
    if sys.platform != "win32":
        raise SystemExit("This bridge only runs on Windows.")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slot", type=int, default=0, help="WinMM joystick ID (default: 0)")
    source = parser.add_mutually_exclusive_group()
    source.add_argument("--any-input", action="store_true", help="All button bits and POV directions (default)")
    source.add_argument("--button", type=int, help="1-based WinMM button number (1-32)")
    source.add_argument("--pov", choices=POV, help="Exact D-pad direction")
    parser.add_argument("--send", action="store_true", help="Send F24 to the active desktop; default is read-only")
    parser.add_argument("--seconds", type=float, default=10, help="Duration; use 0 for continuous operation")
    parser.add_argument("--interval-ms", type=int, default=20, help="Poll interval, minimum 10 ms")
    parser.add_argument("--debounce-ms", type=int, default=0, help="Minimum time between F24 presses")
    args = parser.parse_args()
    if not 0 <= args.slot <= 15 or args.button is not None and not 1 <= args.button <= 32:
        parser.error("slot must be 0-15 and button must be 1-32")
    if args.interval_ms < 10 or args.debounce_ms < 0 or args.seconds < 0:
        parser.error("interval-ms must be >= 10; debounce-ms and seconds must be >= 0")
    any_input = args.any_input or (args.button is None and args.pov is None)

    joy = ct.WinDLL("winmm.dll")
    get_pos = joy.joyGetPosEx
    get_pos.argtypes = [wt.UINT, ct.POINTER(JOYINFOEX)]
    get_pos.restype = wt.UINT
    user32 = ct.WinDLL("user32.dll", use_last_error=True)
    send_input = user32.SendInput
    send_input.argtypes = [wt.UINT, ct.POINTER(INPUT), ct.c_int]
    send_input.restype = wt.UINT

    def press_f24():
        events = (INPUT * 2)(
            INPUT(1, INPUTUNION(ki=KEYBDINPUT(VK_F24, 0, 0, 0, 0))),
            INPUT(1, INPUTUNION(ki=KEYBDINPUT(VK_F24, 0, KEYEVENTF_KEYUP, 0, 0))),
        )
        sent = send_input(2, events, ct.sizeof(INPUT))
        if sent != 2:
            raise OSError(ct.get_last_error(), "SendInput failed (possibly blocked by privilege or game input handling)")

    mode = "SEND F24" if args.send else "READ ONLY"
    print(f"{mode}: WinMM slot {args.slot}, {args.seconds or 'unlimited'} seconds. Ctrl+C stops.", flush=True)
    print("F24 is not guaranteed to count in the pet or be ignored by a game.", flush=True)
    end = time.monotonic() + args.seconds if args.seconds else float("inf")
    old_state = None
    old_pressed = False
    last_sent = float("-inf")
    try:
        while time.monotonic() < end:
            state = JOYINFOEX()
            state.dwSize = ct.sizeof(state)
            state.dwFlags = JOY_RETURNALL
            result = get_pos(args.slot, ct.byref(state))
            if result:
                raise RuntimeError(f"joyGetPosEx({args.slot}) returned {result}; device disconnected or slot unavailable")
            current = (state.dwButtons, state.dwPOV)
            if current != old_state and not args.send:
                print(f"buttons=0x{state.dwButtons:08X} pov={state.dwPOV}", flush=True)
            if any_input:
                edge = input_edge(
                    state.dwButtons, state.dwPOV,
                    old_state[0] if old_state else None,
                    old_state[1] if old_state else POV_CENTER,
                )
            else:
                pressed = (
                    bool(state.dwButtons & (1 << (args.button - 1))) if args.button is not None
                    else state.dwPOV == POV[args.pov]
                )
                edge = pressed and not old_pressed
                old_pressed = pressed
            old_state = current
            now = time.monotonic()
            if args.send and edge and (now - last_sent) * 1000 >= args.debounce_ms:
                press_f24()
                last_sent = now
                print(f"F24 edge: buttons=0x{state.dwButtons:08X} pov={state.dwPOV}", flush=True)
            time.sleep(args.interval_ms / 1000)
    except KeyboardInterrupt:
        print("Stopped.")
    except (RuntimeError, OSError) as exc:
        raise SystemExit(str(exc)) from exc


if __name__ == "__main__":
    main()
