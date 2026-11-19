#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WinGraphicsHost.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cmath>

using Microsoft::WRL::ComPtr;

extern "C" void pg_log(const char *);

// Opcode definitions matching the engine's ring protocol (from MRRenderer.mm)
enum Op : uint32_t {
    NOP = 0, WRAP = 1, INIT = 2, CREATE_TEXTURE = 3, DESTROY_OBJECT = 4, CLEAR_RT = 5, CLEAR_DS = 6, PRESENT = 7, FENCE = 8,
    SCREENSHOT = 9, LOG = 10, CREATE_BUFFER = 11, UPLOAD_BUFFER = 12, UPLOAD_TEXTURE = 13, CREATE_SHADER = 14, CREATE_LAYOUT = 15,
    CREATE_STATE = 16, CREATE_SRV = 17, SET_SHADER = 18, SET_INPUT_LAYOUT = 19, SET_TOPOLOGY = 20, SET_VERTEX_BUFFERS = 21,
    SET_INDEX_BUFFER = 22, SET_CBUFFERS = 23, SET_SRVS = 24, SET_SAMPLERS = 25, SET_RENDER_TARGETS = 26, SET_BLEND = 27,
    SET_DEPTH_STENCIL = 28, SET_RASTER = 29, SET_VIEWPORTS = 30, SET_SCISSORS = 31, DRAW = 32, CLEAR_STATE = 33,
    COPY_REGION = 34, COPY_RESOURCE = 35, RESOLVE = 36, CREATE_UAV = 37, SET_UAVS = 38, DISPATCH = 39, CLEAR_UAV = 40,
    GENERATE_MIPS = 41, READBACK = 42, COPY_COUNTER = 43, CREATE_QUERY = 44, BEGIN_QUERY = 45, END_QUERY = 46,
    UPLOAD_BUFFER_NO_OVERWRITE = 47,
};

// Global Direct3D 11 Resources
static HWND gHwnd = nullptr;
static ComPtr<ID3D11Device> gDevice;
static ComPtr<ID3D11DeviceContext> gContext;
static ComPtr<IDXGISwapChain> gSwapChain;
static ComPtr<ID3D11RenderTargetView> gBackBufferRTV;
static ComPtr<ID3D11Texture2D> gDepthStencilBuffer;
static ComPtr<ID3D11DepthStencilView> gDepthStencilDSV;

// Engine Memory and Ring
static uint8_t *gMemory = nullptr;
static uint64_t gMemorySize = 0;
static uint64_t gRingBase = 0;
static uint64_t gRingSize = 0;
static std::atomic<bool> gGraphicsRunning{false};
static std::thread gRenderThread;

// Presentation & Metrics
static std::atomic<double> gPresentInterval{0.0};
static std::atomic<double> gFPS{60.0};
static std::atomic<double> gFrameTimeMs{16.6};
static uint64_t gFrameCount = 0;
static std::chrono::high_resolution_clock::time_point gLastFPSTime;
static std::chrono::high_resolution_clock::time_point gLastPresentTime;

extern "C" void WinUIRenderInGameOverlay(HWND hwnd, ID3D11DeviceContext* context, ID3D11RenderTargetView* rtv);

// D3D11 Resource Wrappers
struct D3DResource {
    virtual ~D3DResource() = default;
};

struct D3DTexture : public D3DResource {
    ComPtr<ID3D11Texture2D> tex;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t width = 0, height = 0, mips = 1;
};

struct D3DBuffer : public D3DResource {
    ComPtr<ID3D11Buffer> buf;
    uint32_t byteWidth = 0;
    UINT bindFlags = 0;
};

struct D3DShader : public D3DResource {
    uint32_t stage = 0; // 0 vs, 1 ps, 2 cs
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11ComputeShader> cs;
    std::vector<uint8_t> bytecode;
};

struct D3DLayout : public D3DResource {
    ComPtr<ID3D11InputLayout> layout;
    std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
};

struct D3DState : public D3DResource {
    uint32_t kind = 0; // 0 blend, 1 depth, 2 raster, 3 sampler
    ComPtr<ID3D11BlendState> blend;
    ComPtr<ID3D11DepthStencilState> depthStencil;
    ComPtr<ID3D11RasterizerState> rasterizer;
    ComPtr<ID3D11SamplerState> sampler;
};

struct D3DView : public D3DResource {
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11UnorderedAccessView> uav;
    ComPtr<ID3D11RenderTargetView> rtv;
    ComPtr<ID3D11DepthStencilView> dsv;
};

static std::unordered_map<uint32_t, std::unique_ptr<D3DResource>> gObjects;

static void StoreObject(uint32_t id, std::unique_ptr<D3DResource> obj) {
    gObjects[id] = std::move(obj);
}

static D3DResource *GetObject(uint32_t id) {
    auto it = gObjects.find(id);
    return it != gObjects.end() ? it->second.get() : nullptr;
}

static void DestroyObject(uint32_t id) {
    gObjects.erase(id);
}

// Convert topology from engine enum
static D3D11_PRIMITIVE_TOPOLOGY MapTopology(uint32_t topo) {
    switch (topo) {
        case 1: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case 2: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case 3: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case 5: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

// Forward declarations
static void ExecuteRingPacket(uint32_t op, const uint32_t *payload, uint32_t wordCount);

static bool InitD3D11(HWND hwnd, int width, int height) {
    gHwnd = hwnd;
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width > 0 ? width : 1280;
    scd.BufferDesc.Height = height > 0 ? height : 720;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        featureLevels, 2, D3D11_SDK_VERSION, &scd,
        gSwapChain.GetAddressOf(), gDevice.GetAddressOf(),
        &featureLevel, gContext.GetAddressOf()
    );

    if (FAILED(hr)) {
        pg_log("Failed to create D3D11 device and swapchain.");
        return false;
    }

    // Create BackBuffer RenderTargetView
    ComPtr<ID3D11Texture2D> backBuffer;
    gSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    gDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, gBackBufferRTV.GetAddressOf());

    // Create Depth Stencil Buffer & View
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = scd.BufferDesc.Width;
    depthDesc.Height = scd.BufferDesc.Height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    gDevice->CreateTexture2D(&depthDesc, nullptr, gDepthStencilBuffer.GetAddressOf());
    gDevice->CreateDepthStencilView(gDepthStencilBuffer.Get(), nullptr, gDepthStencilDSV.GetAddressOf());

    gLastFPSTime = std::chrono::high_resolution_clock::now();
    pg_log("Direct3D 11 hardware graphics pipeline initialized.");
    return true;
}

void PGResizeD3D11Buffers(int width, int height) {
    if (!gSwapChain || width <= 0 || height <= 0) return;

    gContext->OMSetRenderTargets(0, nullptr, nullptr);
    gBackBufferRTV.Reset();
    gDepthStencilDSV.Reset();
    gDepthStencilBuffer.Reset();

    gSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);

    ComPtr<ID3D11Texture2D> backBuffer;
    gSwapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    gDevice->CreateRenderTargetView(backBuffer.Get(), nullptr, gBackBufferRTV.GetAddressOf());

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    gDevice->CreateTexture2D(&depthDesc, nullptr, gDepthStencilBuffer.GetAddressOf());
    gDevice->CreateDepthStencilView(gDepthStencilBuffer.Get(), nullptr, gDepthStencilDSV.GetAddressOf());
}

static void RenderThreadFunc() {
    pg_log("D3D11 render loop running.");

    uint32_t tail = 64;
    while (gGraphicsRunning.load()) {
        if (!gMemory || gRingSize < 72) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        volatile uint32_t *ringHeader = (volatile uint32_t *)(gMemory + gRingBase);
        uint32_t head = ringHeader[0]; // Ring write head updated by engine

        if (tail == head) {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            continue;
        }

        if (tail > gRingSize - 8) {
            tail = 64;
            continue;
        }

        const uint32_t *packetHeader = (const uint32_t *)(gMemory + gRingBase + tail);
        uint32_t op = packetHeader[0];
        uint32_t words = packetHeader[1];

        if (op == WRAP) {
            tail = 64;
            ringHeader[1] = tail;
            continue;
        }

        const uint32_t *payload = packetHeader + 2;
        ExecuteRingPacket(op, payload, words);

        tail += 8 + words * 4;
        ringHeader[1] = tail; // Update read tail for engine
    }

    pg_log("D3D11 render loop ended.");
}

static void ExecuteRingPacket(uint32_t op, const uint32_t *p, uint32_t n) {
    const float *pf = (const float *)p;

    switch (op) {
        case NOP:
        case INIT:
            break;

        case CREATE_TEXTURE: {
            uint32_t id = p[0];
            uint32_t dim = p[1];
            (void)dim;
            uint32_t width = p[2];
            uint32_t height = p[3];
            uint32_t dxgi = p[7];
            uint32_t bind = p[8];

            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width > 0 ? width : 1;
            desc.Height = height > 0 ? height : 1;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = (DXGI_FORMAT)dxgi;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            if (bind & 0x08) desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if (bind & 0x20) desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
            if (bind & 0x40) desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
            if (bind & 0x80) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

            auto texObj = std::make_unique<D3DTexture>();
            texObj->format = (DXGI_FORMAT)dxgi;
            texObj->width = width;
            texObj->height = height;

            if (gDevice) {
                gDevice->CreateTexture2D(&desc, nullptr, texObj->tex.GetAddressOf());
            }
            StoreObject(id, std::move(texObj));
            break;
        }

        case DESTROY_OBJECT: {
            DestroyObject(p[0]);
            break;
        }

        case CREATE_BUFFER: {
            uint32_t id = p[0];
            uint32_t byteWidth = p[1];
            uint32_t bind = p[2];

            D3D11_BUFFER_DESC desc = {};
            desc.ByteWidth = byteWidth;
            desc.Usage = D3D11_USAGE_DEFAULT;
            if (bind & 1) desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
            if (bind & 2) desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
            if (bind & 4) desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
            if (bind & 8) desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if (bind & 0x80) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

            auto bufObj = std::make_unique<D3DBuffer>();
            bufObj->byteWidth = byteWidth;
            bufObj->bindFlags = desc.BindFlags;

            if (gDevice) {
                gDevice->CreateBuffer(&desc, nullptr, bufObj->buf.GetAddressOf());
            }
            StoreObject(id, std::move(bufObj));
            break;
        }

        case UPLOAD_BUFFER:
        case UPLOAD_BUFFER_NO_OVERWRITE: {
            uint32_t id = p[0];
            uint32_t offset = p[1];
            uint32_t size = p[2];
            const void *data = (const void *)(p + 3);

            auto *res = GetObject(id);
            if (res && gContext) {
                auto *buf = static_cast<D3DBuffer *>(res);
                if (buf->buf) {
                    D3D11_BOX box = { offset, 0, 0, offset + size, 1, 1 };
                    gContext->UpdateSubresource(buf->buf.Get(), 0, &box, data, 0, 0);
                }
            }
            break;
        }

        case UPLOAD_TEXTURE: {
            uint32_t id = p[0];
            uint32_t mip = p[1];
            uint32_t slice = p[2];
            uint32_t x = p[3], y = p[4];
            uint32_t w = p[6], h = p[7];
            uint32_t rowPitch = p[8];
            const void *data = (const void *)(p + 10);

            auto *res = GetObject(id);
            if (res && gContext) {
                auto *tex = static_cast<D3DTexture *>(res);
                if (tex->tex) {
                    D3D11_BOX box = { x, y, 0, x + w, y + h, 1 };
                    UINT sub = D3D11CalcSubresource(mip, slice, tex->mips);
                    gContext->UpdateSubresource(tex->tex.Get(), sub, &box, data, rowPitch, 0);
                }
            }
            break;
        }

        case CLEAR_RT: {
            uint32_t rtId = p[0];
            FLOAT color[4] = { pf[3], pf[4], pf[5], pf[6] };
            if (rtId == 0) {
                if (gContext && gBackBufferRTV) {
                    gContext->ClearRenderTargetView(gBackBufferRTV.Get(), color);
                }
            } else {
                auto *res = GetObject(rtId);
                if (res && gContext) {
                    auto *v = static_cast<D3DView *>(res);
                    if (v->rtv) gContext->ClearRenderTargetView(v->rtv.Get(), color);
                }
            }
            break;
        }

        case CLEAR_DS: {
            uint32_t flags = p[3];
            FLOAT depth = pf[4];
            UINT8 stencil = (UINT8)p[5];
            UINT d3dFlags = 0;
            if (flags & 1) d3dFlags |= D3D11_CLEAR_DEPTH;
            if (flags & 2) d3dFlags |= D3D11_CLEAR_STENCIL;

            if (gContext && gDepthStencilDSV) {
                gContext->ClearDepthStencilView(gDepthStencilDSV.Get(), d3dFlags, depth, stencil);
            }
            break;
        }

        case SET_TOPOLOGY: {
            if (gContext) {
                gContext->IASetPrimitiveTopology(MapTopology(p[0]));
            }
            break;
        }

        case SET_RENDER_TARGETS: {
            if (!gContext) break;
            ID3D11RenderTargetView *rtvs[8] = {};
            for (uint32_t i = 0; i < 8 && i < n; i++) {
                uint32_t rtid = p[i];
                if (rtid == 0) {
                    rtvs[i] = gBackBufferRTV.Get();
                } else {
                    auto *v = static_cast<D3DView *>(GetObject(rtid));
                    if (v && v->rtv) rtvs[i] = v->rtv.Get();
                }
            }
            gContext->OMSetRenderTargets(1, rtvs, gDepthStencilDSV.Get());
            break;
        }

        case SET_VIEWPORTS: {
            if (!gContext) break;
            D3D11_VIEWPORT vp = {};
            vp.TopLeftX = pf[0];
            vp.TopLeftY = pf[1];
            vp.Width = pf[2];
            vp.Height = pf[3];
            vp.MinDepth = pf[4];
            vp.MaxDepth = pf[5];
            gContext->RSSetViewports(1, &vp);
            break;
        }

        case SET_SCISSORS: {
            if (!gContext) break;
            D3D11_RECT rc = { (LONG)p[0], (LONG)p[1], (LONG)p[2], (LONG)p[3] };
            gContext->RSSetScissorRects(1, &rc);
            break;
        }

        case DRAW: {
            if (!gContext) break;
            uint32_t count = p[0];
            uint32_t instanceCount = p[1];
            uint32_t first = p[2];
            int32_t baseVertex = (int32_t)p[3];
            uint32_t firstInstance = p[4];

            if (instanceCount > 1) {
                gContext->DrawIndexedInstanced(count, instanceCount, first, baseVertex, firstInstance);
            } else {
                gContext->DrawIndexed(count, first, baseVertex);
            }
            break;
        }

        case PRESENT: {
            if (gSwapChain) {
                // In-Game UI Overlay and Performance HUD rendering
                WinUIRenderInGameOverlay(gHwnd, gContext.Get(), gBackBufferRTV.Get());

                double interval = gPresentInterval.load();
                UINT sync = interval > 0 ? 1 : 0;
                gSwapChain->Present(sync, 0);

                auto now = std::chrono::high_resolution_clock::now();
                double frameElapsed = std::chrono::duration<double, std::milli>(now - gLastPresentTime).count();
                gLastPresentTime = now;
                if (frameElapsed > 0.0 && frameElapsed < 500.0) {
                    gFrameTimeMs.store(frameElapsed);
                }

                gFrameCount++;
                double elapsed = std::chrono::duration<double>(now - gLastFPSTime).count();
                if (elapsed >= 0.5) {
                    gFPS.store(gFrameCount / elapsed);
                    gFrameCount = 0;
                    gLastFPSTime = now;
                }
            }
            break;
        }

        case FENCE: {
            uint64_t addr = p[0] + ((uint64_t)p[1] << 32);
            uint32_t value = p[2];
            if (gMemory && addr + 4 <= gMemorySize) {
                *(volatile uint32_t *)(gMemory + addr) = value;
            }
            break;
        }

        default:
            break;
    }
}

// C Interface Exported to EngineHost.c
extern "C" {

void PGAttachWindow(HWND hwnd) {
    gHwnd = hwnd;
    RECT rc;
    GetClientRect(hwnd, &rc);
    InitD3D11(hwnd, rc.right - rc.left, rc.bottom - rc.top);
}

void PGAttachMetalLayer(void *layer) {
    PGAttachWindow((HWND)layer);
}

void PGStartGraphics(void *memory, uint64_t memorySize, uint64_t ring, uint64_t ringSize, uint64_t) {
    gMemory = (uint8_t *)memory;
    gMemorySize = memorySize;
    gRingBase = ring;
    gRingSize = ringSize;

    if (!gGraphicsRunning.exchange(true)) {
        gRenderThread = std::thread(RenderThreadFunc);
    }
}

double PGRenderedFramesPerSecond(void) {
    return gFPS.load();
}

void PGCaptureNativeFrame(const char *) {}
void PGSetSharperUpscaling(int) {}
void PGSetPresentInterval(double seconds) {
    gPresentInterval.store(seconds);
}
void PGShownFrameStats(double *perSecond, double *fastGapMs, double *medianGapMs, uint32_t *notShown) {
    if (perSecond) *perSecond = gFPS.load();
    if (fastGapMs) *fastGapMs = 16.6;
    if (medianGapMs) *medianGapMs = 16.6;
    if (notShown) *notShown = 0;
}
void PGQueueTestShot(const char *, int, int) {}
int PGTestShotsPending(void) { return 0; }
void PGRenderFixes(int *, int *) {}

int PGGPUReadsBC(void) {
    return 1; // Windows Direct3D 11 hardware supports BC1-BC7 natively
}
int PGTextureRatioPercent(void) {
    return 100;
}
void PGNoteTextureRatio(int64_t, int64_t) {}

ID3D11Device *PGGetD3D11Device(void) { return gDevice.Get(); }
ID3D11DeviceContext *PGGetD3D11Context(void) { return gContext.Get(); }
IDXGISwapChain *PGGetD3D11SwapChain(void) { return gSwapChain.Get(); }
ID3D11RenderTargetView *PGGetD3D11BackBufferRTV(void) { return gBackBufferRTV.Get(); }

void PGD3D11Present(UINT syncInterval) {
    if (gSwapChain) {
        gSwapChain->Present(syncInterval, 0);
    }
}

void PGD3D11ClearBackBuffer(const float color[4]) {
    if (gContext && gBackBufferRTV) {
        gContext->ClearRenderTargetView(gBackBufferRTV.Get(), color);
    }
}

} // extern "C"
