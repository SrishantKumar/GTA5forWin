#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Direct3D 11 Graphics Host Interface (matching NativeGraphics.h)
void PGAttachWindow(HWND hwnd);
void PGAttachMetalLayer(void *layer); // Compatibility alias
void PGStartGraphics(void *memory, uint64_t memorySize, uint64_t ring, uint64_t ringSize, uint64_t debug);
double PGRenderedFramesPerSecond(void);
double PGGPUFrameMilliseconds(void);
void PGCaptureNativeFrame(const char *name);
void PGSetSharperUpscaling(int on);
void PGSetPresentInterval(double seconds);
void PGShownFrameStats(double *perSecond, double *fastGapMs, double *medianGapMs, uint32_t *notShown);
void PGQueueTestShot(const char *name, int stripLists, int nanClamp);
int PGTestShotsPending(void);
void PGRenderFixes(int *stripLists, int *nanClamp);

int PGGPUReadsBC(void);
int PGTextureRatioPercent(void);
void PGNoteTextureRatio(int64_t gpuBytes, int64_t gameBytes);

// Windows native D3D11 device access
ID3D11Device *PGGetD3D11Device(void);
ID3D11DeviceContext *PGGetD3D11Context(void);
IDXGISwapChain *PGGetD3D11SwapChain(void);
ID3D11RenderTargetView *PGGetD3D11BackBufferRTV(void);
void PGResizeD3D11Buffers(int width, int height);
void PGD3D11Present(UINT syncInterval);
void PGD3D11ClearBackBuffer(const float color[4]);

#ifdef __cplusplus
}
#endif
