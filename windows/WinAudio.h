#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool WinAudioInit(void);
void WinAudioStart(void);
void WinAudioStop(void);
void WinAudioShutdown(void);

#ifdef __cplusplus
}
#endif
