#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WinInput.h"
#include "../app/Source/EngineHost.h"
#include <windows.h>
#include <windowsx.h>
#include <xinput.h>
#include <hidusage.h>
#include <cmath>

static HWND gInputHwnd = nullptr;
static bool gCursorLocked = false;
static int32_t gMouseButtons = 0;
static double gMouseDX = 0.0, gMouseDY = 0.0;
static double gMouseWheel = 0.0;

void WinInputInit(HWND hwnd) {
    gInputHwnd = hwnd;

    // Register Raw Input for high-precision relative mouse deltas
    RAWINPUTDEVICE rid = {};
    rid.usUsagePage = HID_USAGE_PAGE_GENERIC;
    rid.usUsage = HID_USAGE_GENERIC_MOUSE;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;

    RegisterRawInputDevices(&rid, 1, sizeof(rid));
}

void WinInputSetCursorLock(HWND hwnd, bool lock) {
    if (lock == gCursorLocked) return;
    gCursorLocked = lock;

    if (lock) {
        RECT rect;
        GetClientRect(hwnd, &rect);
        MapWindowPoints(hwnd, nullptr, (POINT *)&rect, 2);
        ClipCursor(&rect);
        ShowCursor(FALSE);
        PGSetMouseFocus(true, true);
    } else {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
        PGSetMouseFocus(false, false);
    }
}

bool WinInputIsCursorLocked(void) {
    return gCursorLocked;
}

bool WinInputProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INPUT: {
            UINT dwSize = sizeof(RAWINPUT);
            static BYTE lpb[sizeof(RAWINPUT)];
            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
            RAWINPUT *raw = (RAWINPUT *)lpb;

            if (raw->header.dwType == RIM_TYPEMOUSE && gCursorLocked) {
                int dx = raw->data.mouse.lLastX;
                int dy = raw->data.mouse.lLastY;
                gMouseDX += dx;
                gMouseDY += dy;
            }
            return true;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            bool down = (uMsg == WM_LBUTTONDOWN);
            if (down) gMouseButtons |= 1; else gMouseButtons &= ~1;
            if (!gCursorLocked && down) WinInputSetCursorLock(hwnd, true);
            return true;
        }

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            bool down = (uMsg == WM_RBUTTONDOWN);
            if (down) gMouseButtons |= 2; else gMouseButtons &= ~2;
            return true;
        }

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            bool down = (uMsg == WM_MBUTTONDOWN);
            if (down) gMouseButtons |= 4; else gMouseButtons &= ~4;
            return true;
        }

        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            gMouseWheel += (double)delta / WHEEL_DELTA;
            return true;
        }

        case WM_KEYDOWN:
        case WM_KEYUP: {
            bool down = (uMsg == WM_KEYDOWN);
            uint8_t vk = (uint8_t)wParam;

            if (vk == VK_ESCAPE && down) {
                if (gCursorLocked) {
                    WinInputSetCursorLock(hwnd, false);
                } else {
                    WinInputSetCursorLock(hwnd, true);
                }
            }

            PGSetKey(vk, down);
            PGAsiSetKey((int)vk, down);

            // Handle side-generic Shift, Ctrl, Alt
            if (vk == VK_LSHIFT || vk == VK_RSHIFT) PGSetKey(VK_SHIFT, down);
            if (vk == VK_LCONTROL || vk == VK_RCONTROL) PGSetKey(VK_CONTROL, down);
            if (vk == VK_LMENU || vk == VK_RMENU) PGSetKey(VK_MENU, down);
            return true;
        }

        case WM_ACTIVATE: {
            if (LOWORD(wParam) == WA_INACTIVE) {
                WinInputSetCursorLock(hwnd, false);
                PGReleaseAllKeys();
            }
            return true;
        }
    }
    return false;
}

static float ApplyDeadzone(SHORT value, SHORT deadzone) {
    if (std::abs(value) < deadzone) return 0.0f;
    float norm = (float)value / 32767.0f;
    return norm > 1.0f ? 1.0f : (norm < -1.0f ? -1.0f : norm);
}

void WinInputPoll(double elapsedSeconds) {
    // 1. Dispatch mouse movement & wheel to engine
    if (gMouseDX != 0.0 || gMouseDY != 0.0 || gMouseButtons != 0) {
        int32_t idx = (int32_t)std::round(gMouseDX);
        int32_t idy = (int32_t)std::round(gMouseDY);
        gMouseDX -= idx;
        gMouseDY -= idy;
        PGMouseInput(idx, idy, gMouseButtons);
    }

    if (gMouseWheel != 0.0) {
        int32_t notches = (int32_t)std::round(gMouseWheel);
        if (notches != 0) {
            PGMouseWheel(notches);
            gMouseWheel -= notches;
        }
    }

    // 2. Poll XInput Controller
    XINPUT_STATE xstate;
    if (XInputGetState(0, &xstate) == ERROR_SUCCESS) {
        PGControllerState state = {};
        state.leftX = ApplyDeadzone(xstate.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        state.leftY = ApplyDeadzone(xstate.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        state.rightX = ApplyDeadzone(xstate.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        state.rightY = ApplyDeadzone(xstate.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

        state.leftTrigger = (float)xstate.Gamepad.bLeftTrigger / 255.0f;
        state.rightTrigger = (float)xstate.Gamepad.bRightTrigger / 255.0f;

        WORD w = xstate.Gamepad.wButtons;
        uint32_t buttons = 0;
        if (w & XINPUT_GAMEPAD_A) buttons |= (1 << 0);
        if (w & XINPUT_GAMEPAD_B) buttons |= (1 << 1);
        if (w & XINPUT_GAMEPAD_X) buttons |= (1 << 2);
        if (w & XINPUT_GAMEPAD_Y) buttons |= (1 << 3);
        if (w & XINPUT_GAMEPAD_LEFT_SHOULDER) buttons |= (1 << 4);
        if (w & XINPUT_GAMEPAD_RIGHT_SHOULDER) buttons |= (1 << 5);
        if (w & XINPUT_GAMEPAD_START) buttons |= (1 << 6);
        if (w & XINPUT_GAMEPAD_DPAD_UP) buttons |= (1 << 7);
        if (w & XINPUT_GAMEPAD_DPAD_DOWN) buttons |= (1 << 8);
        if (w & XINPUT_GAMEPAD_DPAD_LEFT) buttons |= (1 << 9);
        if (w & XINPUT_GAMEPAD_DPAD_RIGHT) buttons |= (1 << 10);

        state.buttons = buttons;
        PGUpdateController(state, elapsedSeconds);
    }
}
