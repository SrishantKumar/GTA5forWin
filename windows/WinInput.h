#pragma once
#include <windows.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void WinInputInit(HWND hwnd);
bool WinInputProcessMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void WinInputPoll(double elapsedSeconds);
void WinInputSetCursorLock(HWND hwnd, bool lock);
bool WinInputIsCursorLocked(void);

#ifdef __cplusplus
}
#endif
