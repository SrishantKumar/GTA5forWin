#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WinAudio.h"
#include "../app/Source/EngineHost.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <thread>
#include <atomic>
#include <vector>

using Microsoft::WRL::ComPtr;

extern "C" void pg_log(const char *);

static ComPtr<IAudioClient> gAudioClient;
static ComPtr<IAudioRenderClient> gRenderClient;
static HANDLE gAudioEvent = nullptr;
static std::thread gAudioThread;
static std::atomic<bool> gAudioRunning{false};
static UINT32 gBufferFrameCount = 0;

static void AudioThreadFunc() {
    pg_log("WASAPI low-latency audio stream started.");

    std::vector<float> leftBuffer(2048);
    std::vector<float> rightBuffer(2048);

    while (gAudioRunning.load()) {
        DWORD waitResult = WaitForSingleObject(gAudioEvent, 50);
        if (waitResult != WAIT_OBJECT_0) continue;

        UINT32 numPaddingFrames = 0;
        if (FAILED(gAudioClient->GetCurrentPadding(&numPaddingFrames))) continue;

        UINT32 numAvailableFrames = gBufferFrameCount - numPaddingFrames;
        if (numAvailableFrames == 0) continue;

        if (numAvailableFrames > leftBuffer.size()) {
            leftBuffer.resize(numAvailableFrames);
            rightBuffer.resize(numAvailableFrames);
        }

        BYTE *pData = nullptr;
        if (SUCCEEDED(gRenderClient->GetBuffer(numAvailableFrames, &pData))) {
            // Fill planar audio from GTA V engine
            PGFillAudio(leftBuffer.data(), rightBuffer.data(), numAvailableFrames);

            // Interleave float stereo samples into WASAPI buffer
            float *out = (float *)pData;
            for (UINT32 i = 0; i < numAvailableFrames; i++) {
                out[i * 2 + 0] = leftBuffer[i];
                out[i * 2 + 1] = rightBuffer[i];
            }

            gRenderClient->ReleaseBuffer(numAvailableFrames, 0);
        }
    }

    pg_log("WASAPI audio stream stopped.");
}

bool WinAudioInit(void) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<IMMDeviceEnumerator> enumerator;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(enumerator.GetAddressOf()));
    if (FAILED(hr)) return false;

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)gAudioClient.GetAddressOf());
    if (FAILED(hr)) return false;

    // 48 kHz, 2-channel, 32-bit float PCM
    WAVEFORMATEXTENSIBLE wfx = {};
    wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wfx.Format.nChannels = 2;
    wfx.Format.nSamplesPerSec = 48000;
    wfx.Format.wBitsPerSample = 32;
    wfx.Format.nBlockAlign = wfx.Format.nChannels * (wfx.Format.wBitsPerSample / 8);
    wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
    wfx.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
    wfx.Samples.wValidBitsPerSample = 32;
    wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    REFERENCE_TIME bufferDuration = 1000000; // 100 ms buffer
    hr = gAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        bufferDuration,
        0,
        (WAVEFORMATEX *)&wfx,
        nullptr
    );

    if (FAILED(hr)) {
        pg_log("WASAPI AudioClient Initialize failed.");
        return false;
    }

    gAudioEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    hr = gAudioClient->SetEventHandle(gAudioEvent);
    if (FAILED(hr)) return false;

    hr = gAudioClient->GetBufferSize(&gBufferFrameCount);
    if (FAILED(hr)) return false;

    hr = gAudioClient->GetService(IID_PPV_ARGS(gRenderClient.GetAddressOf()));
    if (FAILED(hr)) return false;

    return true;
}

void WinAudioStart(void) {
    if (!gAudioClient) return;
    if (!gAudioRunning.exchange(true)) {
        gAudioClient->Start();
        gAudioThread = std::thread(AudioThreadFunc);
    }
}

void WinAudioStop(void) {
    if (gAudioRunning.exchange(false)) {
        if (gAudioThread.joinable()) {
            SetEvent(gAudioEvent);
            gAudioThread.join();
        }
        if (gAudioClient) {
            gAudioClient->Stop();
        }
    }
}

void WinAudioShutdown(void) {
    WinAudioStop();
    if (gAudioEvent) {
        CloseHandle(gAudioEvent);
        gAudioEvent = nullptr;
    }
    gRenderClient.Reset();
    gAudioClient.Reset();
}
