/* Standalone, Zero-CRT, Zero-Recursion XInput 1.4 proxy via WinMM (V4.1).
 * - Single file: no xinput1_4_system.dll, no xinput9_1_0.dll recursion.
 * - Zero CRT: imports ONLY KERNEL32.dll (works on Win7/8/10/11 x64).
 * - Full support: 0..15 multi-slot scan, buttons, analog triggers, analog sticks,
 *   auto-adapting to both Xbox (5-axis) and DualSense/DS4/Switch Pro (6/4-axis).
 */

#define WINAPI          __stdcall
#define CALLBACK        __stdcall
#define DECLSPEC_IMPORT __declspec(dllimport)
#define NOINLINE        __attribute__((noinline))

typedef int                BOOL;
typedef unsigned char      BYTE;
typedef unsigned short     WORD;
typedef unsigned long      DWORD;
typedef unsigned int       UINT;
typedef short              SHORT;
typedef long long          LONGLONG;
typedef unsigned long long SIZE_T;
typedef unsigned long long UINT_PTR;
typedef UINT               MMRESULT;
typedef __WCHAR_TYPE__     WCHAR;
typedef void               VOID;
typedef void              *PVOID;
typedef void              *LPVOID;
typedef void              *HANDLE;
typedef void              *HINSTANCE;
typedef void              *HMODULE;
typedef const char        *LPCSTR;
typedef WCHAR             *LPWSTR;
typedef const WCHAR       *LPCWSTR;
typedef long long (WINAPI *FARPROC)(void);

typedef struct { void *Ptr; } INIT_ONCE, *PINIT_ONCE;
typedef struct { void *Ptr; } SRWLOCK, *PSRWLOCK;
typedef BOOL (CALLBACK *PINIT_ONCE_FN)(PINIT_ONCE, PVOID, PVOID *);

#define INIT_ONCE_STATIC_INIT {0}
#define SRWLOCK_INIT          {0}

#define TRUE  1
#define FALSE 0
#define NULL  ((void *)0)
#define MAX_PATH 260

#define ERROR_SUCCESS                0UL
#define ERROR_BAD_ARGUMENTS          160UL
#define DEVICE_NOT_CONNECTED         1167UL
#define ERROR_EMPTY_LOCAL            4306UL
#define JOY_RETURNALL_LOCAL          0x00ffUL
#define MAX_WINMM_SLOTS              16U
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800UL

#define JOYCAPS_HASZ 0x0001U
#define JOYCAPS_HASR 0x0002U
#define JOYCAPS_HASU 0x0004U
#define JOYCAPS_HASV 0x0008U

#define XINPUT_GAMEPAD_DPAD_UP        0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN      0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT      0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT     0x0008
#define XINPUT_GAMEPAD_START          0x0010
#define XINPUT_GAMEPAD_BACK           0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB     0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB    0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER  0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER 0x0200
#define XINPUT_GAMEPAD_A              0x1000
#define XINPUT_GAMEPAD_B              0x2000
#define XINPUT_GAMEPAD_X              0x4000
#define XINPUT_GAMEPAD_Y              0x8000

typedef struct {
    DWORD dwSize, dwFlags;
    DWORD dwXpos, dwYpos, dwZpos, dwRpos, dwUpos, dwVpos;
    DWORD dwButtons, dwButtonNumber, dwPOV;
    DWORD dwReserved1, dwReserved2;
} JOYINFOEX, *LPJOYINFOEX;

typedef struct {
    WORD  wMid, wPid;
    WCHAR szPname[32];
    UINT  wXmin, wXmax, wYmin, wYmax, wZmin, wZmax;
    UINT  wNumButtons, wPeriodMin, wPeriodMax;
    UINT  wRmin, wRmax, wUmin, wUmax, wVmin, wVmax;
    UINT  wCaps, wMaxAxes, wNumAxes, wMaxButtons;
    WCHAR szRegKey[32];
    WCHAR szOEMVxD[260];
} JOYCAPSW, *LPJOYCAPSW;

typedef struct {
    UINT wCaps;
    UINT wXmin, wXmax, wYmin, wYmax, wZmin, wZmax;
    UINT wRmin, wRmax, wUmin, wUmax, wVmin, wVmax;
    BOOL valid;
} SLOT_CAPS;

typedef struct {
    WORD wButtons;
    BYTE bLeftTrigger, bRightTrigger;
    SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} GAMEPAD_LOCAL;

typedef struct {
    DWORD dwPacketNumber;
    GAMEPAD_LOCAL Gamepad;
} STATE_LOCAL;

typedef struct {
    WORD wLeftMotorSpeed, wRightMotorSpeed;
} VIBRATION_LOCAL;

typedef struct {
    BYTE Type, SubType;
    WORD Flags;
    GAMEPAD_LOCAL Gamepad;
    VIBRATION_LOCAL Vibration;
} CAPS_LOCAL;

typedef struct {
    BYTE BatteryType;
    BYTE BatteryLevel;
} BATTERY_LOCAL;

DECLSPEC_IMPORT UINT    WINAPI GetSystemDirectoryW(LPWSTR lpBuffer, UINT uSize);
DECLSPEC_IMPORT HMODULE WINAPI LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
DECLSPEC_IMPORT HMODULE WINAPI LoadLibraryW(LPCWSTR lpLibFileName);
DECLSPEC_IMPORT FARPROC WINAPI GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
DECLSPEC_IMPORT BOOL    WINAPI InitOnceExecuteOnce(PINIT_ONCE InitOnce, PINIT_ONCE_FN InitFn, PVOID Parameter, LPVOID *Context);
DECLSPEC_IMPORT VOID    WINAPI AcquireSRWLockExclusive(PSRWLOCK SRWLock);
DECLSPEC_IMPORT VOID    WINAPI ReleaseSRWLockExclusive(PSRWLOCK SRWLock);

typedef MMRESULT (WINAPI *GetJoyFn)(UINT, LPJOYINFOEX);
typedef MMRESULT (WINAPI *GetJoyCapsFn)(UINT_PTR, LPJOYCAPSW, UINT);

static INIT_ONCE g_once = INIT_ONCE_STATIC_INIT;
static SRWLOCK g_lock = SRWLOCK_INIT;
static GetJoyFn g_joy_get;
static GetJoyCapsFn g_joy_caps;
static SLOT_CAPS g_caps[MAX_WINMM_SLOTS];
static JOYCAPSW g_raw_caps_buf;
static GAMEPAD_LOCAL g_previous;
static DWORD g_packet;
static BOOL g_was_connected;

static void zero_mem(void *dst, SIZE_T n) {
    volatile BYTE *p = (volatile BYTE *)dst;
    while (n--) *p++ = 0;
}

static void copy_mem(void *dst, const void *src, SIZE_T n) {
    volatile BYTE *d = (volatile BYTE *)dst;
    const volatile BYTE *s = (const volatile BYTE *)src;
    while (n--) *d++ = *s++;
}

static BOOL same_mem(const void *a, const void *b, SIZE_T n) {
    const BYTE *pa = (const BYTE *)a;
    const BYTE *pb = (const BYTE *)b;
    while (n--) {
        if (*pa++ != *pb++) return FALSE;
    }
    return TRUE;
}

static NOINLINE HMODULE load_system(const WCHAR *dll) {
    WCHAR path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    SIZE_T length = 0;
    HMODULE mod;
    while (dll[length]) ++length;
    if (!n || (SIZE_T)n + length + 2 > MAX_PATH) return NULL;
    path[n++] = L'\\';
    copy_mem(path + n, dll, (length + 1) * sizeof(WCHAR));
    mod = LoadLibraryExW(path, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!mod) mod = LoadLibraryW(path);
    return mod;
}

static BOOL CALLBACK initialize(PINIT_ONCE once, PVOID param, PVOID *context) {
    HMODULE winmm = load_system(L"winmm.dll");
    (void)once; (void)param; (void)context;
    if (winmm) {
        g_joy_get = (GetJoyFn)GetProcAddress(winmm, "joyGetPosEx");
        g_joy_caps = (GetJoyCapsFn)GetProcAddress(winmm, "joyGetDevCapsW");
    }
    return TRUE;
}

static SHORT scale_axis(DWORD raw, UINT min_v, UINT max_v, BOOL invert) {
    LONGLONG span, val, scaled;
    if (max_v <= min_v) return 0;
    if (raw < (DWORD)min_v) raw = (DWORD)min_v;
    if (raw > (DWORD)max_v) raw = (DWORD)max_v;
    span = (LONGLONG)max_v - (LONGLONG)min_v;
    val = (LONGLONG)raw - (LONGLONG)min_v;
    scaled = (val * 65535LL + (span / 2)) / span;
    scaled = invert ? (32767LL - scaled) : (scaled - 32768LL);
    if (scaled < -32768LL) scaled = -32768LL;
    if (scaled > 32767LL) scaled = 32767LL;
    return (SHORT)scaled;
}

static BYTE scale_trigger(DWORD raw, UINT min_v, UINT max_v) {
    LONGLONG span, val, scaled;
    if (max_v <= min_v) return 0;
    if (raw < (DWORD)min_v) raw = (DWORD)min_v;
    if (raw > (DWORD)max_v) raw = (DWORD)max_v;
    span = (LONGLONG)max_v - (LONGLONG)min_v;
    val = (LONGLONG)raw - (LONGLONG)min_v;
    scaled = (val * 255LL + (span / 2)) / span;
    if (scaled < 0LL) scaled = 0LL;
    if (scaled > 255LL) scaled = 255LL;
    return (BYTE)scaled;
}

static void scale_xbox_triggers(DWORD raw, UINT min_v, UINT max_v, BYTE *lt, BYTE *rt) {
    LONGLONG mid, half, delta, scaled;
    if (max_v <= min_v) return;
    if (raw < (DWORD)min_v) raw = (DWORD)min_v;
    if (raw > (DWORD)max_v) raw = (DWORD)max_v;
    mid = ((LONGLONG)min_v + (LONGLONG)max_v) / 2;
    half = ((LONGLONG)max_v - (LONGLONG)min_v) / 2;
    if (half <= 0) return;
    if ((LONGLONG)raw > mid) {
        delta = (LONGLONG)raw - mid;
        scaled = (delta * 255LL + (half / 2)) / half;
        if (scaled > 255LL) scaled = 255LL;
        if ((BYTE)scaled > *lt) *lt = (BYTE)scaled;
    } else if ((LONGLONG)raw < mid) {
        delta = mid - (LONGLONG)raw;
        scaled = (delta * 255LL + (half / 2)) / half;
        if (scaled > 255LL) scaled = 255LL;
        if ((BYTE)scaled > *rt) *rt = (BYTE)scaled;
    }
}

static SHORT pick_axis(SHORT a, SHORT b) {
    int abs_a = (int)a < 0 ? -(int)a : (int)a;
    int abs_b = (int)b < 0 ? -(int)b : (int)b;
    return abs_b > abs_a ? b : a;
}

static void convert(const JOYINFOEX *joy, const SLOT_CAPS *caps, GAMEPAD_LOCAL *out) {
    static const WORD ps_map[14] = {
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
    static const WORD xbox_map[10] = {
        XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X,
        XINPUT_GAMEPAD_Y, XINPUT_GAMEPAD_LEFT_SHOULDER,
        XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK,
        XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_LEFT_THUMB,
        XINPUT_GAMEPAD_RIGHT_THUMB
    };
    BOOL is_xbox = caps && caps->valid &&
                   ((caps->wCaps & JOYCAPS_HASU) != 0) &&
                   ((caps->wCaps & JOYCAPS_HASV) == 0);
    unsigned i;

    zero_mem(out, sizeof(*out));
    if (is_xbox) {
        for (i = 0; i < 10; ++i) {
            if (joy->dwButtons & (1UL << i)) out->wButtons |= xbox_map[i];
        }
    } else {
        for (i = 0; i < 14; ++i) {
            if (joy->dwButtons & (1UL << i)) out->wButtons |= ps_map[i];
        }
        if (joy->dwButtons & (1UL << 6)) out->bLeftTrigger = 255;
        if (joy->dwButtons & (1UL << 7)) out->bRightTrigger = 255;
    }

    if (joy->dwPOV != 65535UL && joy->dwPOV < 36000UL) {
        DWORD angle = joy->dwPOV;
        if (angle >= 31500UL || angle <= 4500UL) out->wButtons |= XINPUT_GAMEPAD_DPAD_UP;
        if (angle >= 4500UL && angle <= 13500UL) out->wButtons |= XINPUT_GAMEPAD_DPAD_RIGHT;
        if (angle >= 13500UL && angle <= 22500UL) out->wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (angle >= 22500UL && angle <= 31500UL) out->wButtons |= XINPUT_GAMEPAD_DPAD_LEFT;
    }

    if (caps && caps->valid) {
        BOOL has_z = (caps->wCaps & JOYCAPS_HASZ) != 0;
        BOOL has_r = (caps->wCaps & JOYCAPS_HASR) != 0;
        BOOL has_u = (caps->wCaps & JOYCAPS_HASU) != 0;
        BOOL has_v = (caps->wCaps & JOYCAPS_HASV) != 0;

        out->sThumbLX = scale_axis(joy->dwXpos, caps->wXmin, caps->wXmax, FALSE);
        out->sThumbLY = scale_axis(joy->dwYpos, caps->wYmin, caps->wYmax, TRUE);

        if (is_xbox) {
            out->sThumbRX = scale_axis(joy->dwUpos, caps->wUmin, caps->wUmax, FALSE);
            if (has_r) out->sThumbRY = scale_axis(joy->dwRpos, caps->wRmin, caps->wRmax, TRUE);
            if (has_z) scale_xbox_triggers(joy->dwZpos, caps->wZmin, caps->wZmax,
                                           &out->bLeftTrigger, &out->bRightTrigger);
        } else {
            if (has_z) out->sThumbRX = scale_axis(joy->dwZpos, caps->wZmin, caps->wZmax, FALSE);
            if (has_r) out->sThumbRY = scale_axis(joy->dwRpos, caps->wRmin, caps->wRmax, TRUE);
            if (has_u && has_v) {
                BYTE lt = scale_trigger(joy->dwUpos, caps->wUmin, caps->wUmax);
                BYTE rt = scale_trigger(joy->dwVpos, caps->wVmin, caps->wVmax);
                if (lt > out->bLeftTrigger) out->bLeftTrigger = lt;
                if (rt > out->bRightTrigger) out->bRightTrigger = rt;
            }
        }
    } else {
        out->sThumbLX = scale_axis(joy->dwXpos, 0U, 65535U, FALSE);
        out->sThumbLY = scale_axis(joy->dwYpos, 0U, 65535U, TRUE);
        out->sThumbRX = scale_axis(joy->dwZpos, 0U, 65535U, FALSE);
        out->sThumbRY = scale_axis(joy->dwRpos, 0U, 65535U, TRUE);
    }
}

static BOOL get_slot_caps(UINT slot, SLOT_CAPS *out) {
    BOOL ok = FALSE;
    AcquireSRWLockExclusive(&g_lock);
    if (g_caps[slot].valid) {
        copy_mem(out, &g_caps[slot], sizeof(*out));
        ok = TRUE;
    } else if (g_joy_caps) {
        zero_mem(&g_raw_caps_buf, sizeof(g_raw_caps_buf));
        if (g_joy_caps((UINT_PTR)slot, &g_raw_caps_buf, (UINT)sizeof(g_raw_caps_buf)) == 0) {
            SLOT_CAPS *c = &g_caps[slot];
            c->wCaps = g_raw_caps_buf.wCaps;
            c->wXmin = g_raw_caps_buf.wXmin; c->wXmax = g_raw_caps_buf.wXmax;
            c->wYmin = g_raw_caps_buf.wYmin; c->wYmax = g_raw_caps_buf.wYmax;
            c->wZmin = g_raw_caps_buf.wZmin; c->wZmax = g_raw_caps_buf.wZmax;
            c->wRmin = g_raw_caps_buf.wRmin; c->wRmax = g_raw_caps_buf.wRmax;
            c->wUmin = g_raw_caps_buf.wUmin; c->wUmax = g_raw_caps_buf.wUmax;
            c->wVmin = g_raw_caps_buf.wVmin; c->wVmax = g_raw_caps_buf.wVmax;
            c->valid = TRUE;
            copy_mem(out, c, sizeof(*out));
            ok = TRUE;
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
    return ok;
}

static BOOL read_all_joysticks(GAMEPAD_LOCAL *merged) {
    UINT slot;
    BOOL any_connected = FALSE;
    zero_mem(merged, sizeof(*merged));
    if (!g_joy_get) return FALSE;
    for (slot = 0; slot < MAX_WINMM_SLOTS; ++slot) {
        JOYINFOEX joy;
        zero_mem(&joy, sizeof(joy));
        joy.dwSize = sizeof(joy);
        joy.dwFlags = JOY_RETURNALL_LOCAL;
        if (g_joy_get(slot, &joy) == 0) {
            SLOT_CAPS caps;
            GAMEPAD_LOCAL cur;
            BOOL has_caps = get_slot_caps(slot, &caps);
            convert(&joy, has_caps ? &caps : NULL, &cur);
            any_connected = TRUE;
            merged->wButtons |= cur.wButtons;
            if (cur.bLeftTrigger > merged->bLeftTrigger)
                merged->bLeftTrigger = cur.bLeftTrigger;
            if (cur.bRightTrigger > merged->bRightTrigger)
                merged->bRightTrigger = cur.bRightTrigger;
            merged->sThumbLX = pick_axis(merged->sThumbLX, cur.sThumbLX);
            merged->sThumbLY = pick_axis(merged->sThumbLY, cur.sThumbLY);
            merged->sThumbRX = pick_axis(merged->sThumbRX, cur.sThumbRX);
            merged->sThumbRY = pick_axis(merged->sThumbRY, cur.sThumbRY);
        } else {
            AcquireSRWLockExclusive(&g_lock);
            g_caps[slot].valid = FALSE;
            ReleaseSRWLockExclusive(&g_lock);
        }
    }
    return any_connected;
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    (void)hinst; (void)reason; (void)reserved;
    return TRUE;
}

DWORD WINAPI XInputGetState(DWORD index, STATE_LOCAL *state) {
    GAMEPAD_LOCAL pad;
    if (!state) return ERROR_BAD_ARGUMENTS;
    InitOnceExecuteOnce(&g_once, initialize, NULL, NULL);
    if (index != 0) return DEVICE_NOT_CONNECTED;

    if (!read_all_joysticks(&pad)) {
        AcquireSRWLockExclusive(&g_lock);
        g_was_connected = FALSE;
        ReleaseSRWLockExclusive(&g_lock);
        return DEVICE_NOT_CONNECTED;
    }

    AcquireSRWLockExclusive(&g_lock);
    if (!g_was_connected || !same_mem(&pad, &g_previous, sizeof(pad))) {
        ++g_packet;
        copy_mem(&g_previous, &pad, sizeof(pad));
        g_was_connected = TRUE;
    }
    state->dwPacketNumber = g_packet;
    ReleaseSRWLockExclusive(&g_lock);
    copy_mem(&state->Gamepad, &pad, sizeof(pad));
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputSetState(DWORD index, VIBRATION_LOCAL *vibration) {
    GAMEPAD_LOCAL dummy;
    if (!vibration) return ERROR_BAD_ARGUMENTS;
    InitOnceExecuteOnce(&g_once, initialize, NULL, NULL);
    if (index != 0) return DEVICE_NOT_CONNECTED;
    return read_all_joysticks(&dummy) ? ERROR_SUCCESS : DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetCapabilities(DWORD index, DWORD flags, CAPS_LOCAL *caps) {
    GAMEPAD_LOCAL dummy;
    if (!caps) return ERROR_BAD_ARGUMENTS;
    InitOnceExecuteOnce(&g_once, initialize, NULL, NULL);
    if (index != 0) return DEVICE_NOT_CONNECTED;
    if (flags != 0 && flags != 1) return ERROR_BAD_ARGUMENTS;
    if (!read_all_joysticks(&dummy)) return DEVICE_NOT_CONNECTED;
    zero_mem(caps, sizeof(*caps));
    caps->Type = 1;
    caps->SubType = 1;
    caps->Gamepad.wButtons = 0xf3ff;
    caps->Gamepad.bLeftTrigger = 255;
    caps->Gamepad.bRightTrigger = 255;
    caps->Gamepad.sThumbLX = (SHORT)0xffc0;
    caps->Gamepad.sThumbLY = (SHORT)0xffc0;
    caps->Gamepad.sThumbRX = (SHORT)0xffc0;
    caps->Gamepad.sThumbRY = (SHORT)0xffc0;
    return ERROR_SUCCESS;
}

void WINAPI XInputEnable(BOOL enable) {
    (void)enable;
}

DWORD WINAPI XInputGetBatteryInformation(DWORD index, BYTE devType, BATTERY_LOCAL *info) {
    STATE_LOCAL st;
    (void)devType;
    if (!info) return ERROR_BAD_ARGUMENTS;
    zero_mem(info, sizeof(*info));
    if (XInputGetState(index, &st) != ERROR_SUCCESS) return DEVICE_NOT_CONNECTED;
    info->BatteryType = 1;
    info->BatteryLevel = 3;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputGetKeystroke(DWORD index, DWORD reserved, LPVOID keystroke) {
    STATE_LOCAL st;
    (void)reserved;
    if (!keystroke) return ERROR_BAD_ARGUMENTS;
    if (index == 0x000000ffUL) index = 0;
    if (XInputGetState(index, &st) != ERROR_SUCCESS) return DEVICE_NOT_CONNECTED;
    return ERROR_EMPTY_LOCAL;
}

DWORD WINAPI XInputGetAudioDeviceIds(DWORD index, LPWSTR renderId, UINT *renderCount,
                                     LPWSTR captureId, UINT *captureCount) {
    STATE_LOCAL st;
    (void)renderId; (void)captureId;
    if (XInputGetState(index, &st) != ERROR_SUCCESS) return DEVICE_NOT_CONNECTED;
    if (renderCount) *renderCount = 0;
    if (captureCount) *captureCount = 0;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputGetStateEx(DWORD index, STATE_LOCAL *state) {
    return XInputGetState(index, state);
}

DWORD WINAPI XInputWaitForGuideButton(DWORD index, DWORD flag, LPVOID unkn) {
    (void)index; (void)flag; (void)unkn;
    return DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputCancelGuideButtonWait(DWORD index) {
    (void)index;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputPowerOffController(DWORD index) {
    (void)index;
    return ERROR_SUCCESS;
}

DWORD WINAPI XInputGetBaseBusInformation(DWORD index, LPVOID info) {
    (void)index; (void)info;
    return DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputGetCapabilitiesEx(DWORD reserved, DWORD index, DWORD flags, LPVOID capsEx) {
    (void)reserved; (void)index; (void)flags; (void)capsEx;
    return DEVICE_NOT_CONNECTED;
}

DWORD WINAPI XInputOrdinal109(DWORD index, LPVOID arg) {
    (void)index; (void)arg;
    return DEVICE_NOT_CONNECTED;
}
