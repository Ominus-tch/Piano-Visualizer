#include "AudioOutput.h"

#include "../../util/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace audio
{

    AudioOutput::AudioOutput()
        : _device(nullptr),
        _audioClient(nullptr),
        _renderClient(nullptr),
        _format(nullptr),
        _bufferFrames(0),
        _sampleFormat(SampleFormat::Unknown),
        _initialized(false),
        _running(false),
        _comInitialized(false)
    {
        Logger::Log("[Audio] AudioOutput created\n");
    }


    AudioOutput::~AudioOutput()
    {
        shutdown();

        Logger::Log("[Audio] AudioOutput destroyed\n");
    }


    bool AudioOutput::initialize(
        int32_t channels)
    {
        if (_initialized)
        {
            Logger::Log(
                "[Audio] Already initialized\n");

            return true;
        }

        if (channels != 2)
        {
            Logger::Log(
                "[Audio] Only stereo output is currently supported\n");

            return false;
        }

        Logger::Log(
            "[Audio] Initializing WASAPI\n");

        /*
         * Initialize COM for this thread.
         */
        HRESULT hr = CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

        if (FAILED(hr) &&
            hr != RPC_E_CHANGED_MODE)
        {
            Logger::Log(
                "[Audio] CoInitializeEx failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            return false;
        }

        if (hr == S_OK ||
            hr == S_FALSE)
        {
            _comInitialized = true;
        }

        /*
         * Create the WASAPI device enumerator.
         */
        IMMDeviceEnumerator* enumerator = nullptr;

        hr = CoCreateInstance(
            __uuidof(MMDeviceEnumerator),
            nullptr,
            CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator),
            reinterpret_cast<void**>(&enumerator));

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] Failed to create device enumerator: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        /*
         * Get the default playback device.
         */
        hr = enumerator->GetDefaultAudioEndpoint(
            eRender,
            eConsole,
            &_device);

        enumerator->Release();

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] Failed to get default audio endpoint: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        Logger::Log(
            "[Audio] Default audio endpoint acquired\n");

        /*
         * Activate IAudioClient.
         */
        hr = _device->Activate(
            __uuidof(IAudioClient),
            CLSCTX_ALL,
            nullptr,
            reinterpret_cast<void**>(&_audioClient));

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] Failed to activate IAudioClient: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        /*
         * Get the device's actual shared-mode mix format.
         *
         * IMPORTANT:
         *
         * We must NOT modify this format into another format.
         *
         * Windows guarantees that this format can be used
         * to initialize a shared-mode stream.
         */
        hr = _audioClient->GetMixFormat(&_format);

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] GetMixFormat failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        Logger::Log(
            "[Audio] Device format: channels=%u, sampleRate=%u, bits=%u\n",
            _format->nChannels,
            _format->nSamplesPerSec,
            _format->wBitsPerSample);

        /*
         * Determine the actual sample format.
         */
        if (_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            if (_format->wBitsPerSample == 32)
            {
                _sampleFormat = SampleFormat::Float32;
            }
        }
        else if (_format->wFormatTag == WAVE_FORMAT_PCM)
        {
            if (_format->wBitsPerSample == 16)
            {
                _sampleFormat = SampleFormat::PCM16;
            }
            else if (_format->wBitsPerSample == 24)
            {
                _sampleFormat = SampleFormat::PCM24;
            }
            else if (_format->wBitsPerSample == 32)
            {
                _sampleFormat = SampleFormat::PCM32;
            }
        }
        else if (_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            auto* extensible =
                reinterpret_cast<WAVEFORMATEXTENSIBLE*>(_format);

            if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
            {
                if (_format->wBitsPerSample == 32)
                {
                    _sampleFormat = SampleFormat::Float32;
                }
            }
            else if (extensible->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
            {
                if (_format->wBitsPerSample == 16)
                {
                    _sampleFormat = SampleFormat::PCM16;
                }
                else if (_format->wBitsPerSample == 24)
                {
                    _sampleFormat = SampleFormat::PCM24;
                }
                else if (_format->wBitsPerSample == 32)
                {
                    _sampleFormat = SampleFormat::PCM32;
                }
            }
        }

        if (_sampleFormat == SampleFormat::Unknown)
        {
            Logger::Log(
                "[Audio] Unsupported WASAPI sample format\n");

            shutdown();
            return false;
        }

        const char* formatName = "Unknown";

        switch (_sampleFormat)
        {
        case SampleFormat::Float32:
            formatName = "32-bit float";
            break;

        case SampleFormat::PCM16:
            formatName = "16-bit PCM";
            break;

        case SampleFormat::PCM24:
            formatName = "24-bit PCM";
            break;

        case SampleFormat::PCM32:
            formatName = "32-bit PCM";
            break;

        default:
            break;
        }

        /*
         * We deliberately do NOT call IsFormatSupported() with a
         * modified format here.
         *
         * GetMixFormat() gives us the format used by the Windows
         * audio engine for shared-mode processing, and that exact
         * format is valid for Initialize().
         */

        //constexpr REFERENCE_TIME bufferDuration = 30000; // 3 ms
        constexpr REFERENCE_TIME bufferDuration = 200000; // 20 ms

        hr = _audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            0,
            bufferDuration,
            0,
            _format,
            nullptr);

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] IAudioClient::Initialize failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        /*
         * Get actual WASAPI buffer size.
         */
        hr = _audioClient->GetBufferSize(
            &_bufferFrames);

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] GetBufferSize failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        Logger::Log(
            "[Audio] WASAPI buffer: %u frames\n",
            _bufferFrames);

        /*
         * Get render client.
         */
        hr = _audioClient->GetService(
            __uuidof(IAudioRenderClient),
            reinterpret_cast<void**>(&_renderClient));

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] Failed to get IAudioRenderClient: 0x%08X\n",
                static_cast<unsigned>(hr));

            shutdown();
            return false;
        }

        _initialized = true;

        Logger::Log(
            "[Audio] WASAPI initialized successfully\n");

        Logger::Log(
            "[Audio] Actual output sample rate: %u Hz\n",
            _format->nSamplesPerSec);

        return true;
    }


    void AudioOutput::shutdown()
    {
        if (_running)
            stop();

        if (_renderClient)
        {
            _renderClient->Release();
            _renderClient = nullptr;
        }

        if (_audioClient)
        {
            _audioClient->Release();
            _audioClient = nullptr;
        }

        if (_device)
        {
            _device->Release();
            _device = nullptr;
        }

        if (_format)
        {
            CoTaskMemFree(_format);
            _format = nullptr;
        }

        _bufferFrames = 0;

        _sampleFormat = SampleFormat::Unknown;

        _initialized = false;

        if (_comInitialized)
        {
            CoUninitialize();
            _comInitialized = false;
        }

        Logger::Log(
            "[Audio] WASAPI shut down\n");
    }


    bool AudioOutput::start()
    {
        if (!_initialized)
        {
            Logger::Log(
                "[Audio] Cannot start: not initialized\n");

            return false;
        }

        if (_running)
            return true;

        HRESULT hr = _audioClient->Start();

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] AudioClient::Start failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            return false;
        }

        _running = true;

        Logger::Log(
            "[Audio] Audio output started\n");

        return true;
    }


    void AudioOutput::stop()
    {
        if (!_running)
            return;

        HRESULT hr = _audioClient->Stop();

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] AudioClient::Stop failed: 0x%08X\n",
                static_cast<unsigned>(hr));
        }

        _running = false;

        Logger::Log(
            "[Audio] Audio output stopped\n");
    }

    bool AudioOutput::waitForSpace(int32_t frames)
    {
        if (!_initialized ||
            !_running ||
            !_audioClient)
        {
            return false;
        }

        if (frames <= 0)
            return true;

        while (true)
        {
            UINT32 padding = 0;

            HRESULT hr =
                _audioClient->GetCurrentPadding(&padding);

            if (FAILED(hr))
            {
                Logger::Log(
                    "[Audio] GetCurrentPadding failed: 0x%08X\n",
                    static_cast<unsigned>(hr));

                return false;
            }

            const UINT32 available =
                _bufferFrames > padding
                ? _bufferFrames - padding
                : 0;

            if (available >= static_cast<UINT32>(frames))
                return true;

            /*
             * We don't want to spin at 100% CPU while waiting
             * for the audio device to consume samples.
             *
             * A 1 ms sleep is sufficiently small for this
             * test and keeps the CPU usage low.
             */
            Sleep(1);
        }
    }

    bool AudioOutput::write(
        const float* left,
        const float* right,
        int32_t numSamples)
    {
        if (!_initialized ||
            !_running ||
            !_renderClient)
        {
            return false;
        }

        if (!left ||
            !right ||
            numSamples <= 0)
        {
            return false;
        }

        UINT32 framesToWrite =
            static_cast<UINT32>(numSamples);

        UINT32 padding = 0;

        HRESULT hr =
            _audioClient->GetCurrentPadding(&padding);

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] GetCurrentPadding failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            return false;
        }

        UINT32 available =
            _bufferFrames > padding
            ? _bufferFrames - padding
            : 0;

        framesToWrite =
            (std::min)(
                framesToWrite,
                available);

        if (framesToWrite == 0)
            return true;

        BYTE* data = nullptr;

        hr = _renderClient->GetBuffer(
            framesToWrite,
            &data);

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] GetBuffer failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            return false;
        }

        /*
         * Convert our VST float samples into the device's
         * actual WASAPI format.
         */
        switch (_sampleFormat)
        {
        case SampleFormat::Float32:
        {
            float* output =
                reinterpret_cast<float*>(data);

            for (UINT32 i = 0; i < framesToWrite; ++i)
            {
                output[i * 2 + 0] = left[i];
                output[i * 2 + 1] = right[i];
            }

            break;
        }

        case SampleFormat::PCM16:
        {
            auto* output =
                reinterpret_cast<int16_t*>(data);

            for (UINT32 i = 0; i < framesToWrite; ++i)
            {
                const float l =
                    (std::max)(-1.0f, (std::min)(1.0f, left[i]));

                const float r =
                    (std::max)(-1.0f, (std::min)(1.0f, right[i]));

                output[i * 2 + 0] =
                    static_cast<int16_t>(
                        std::lrintf(l * 32767.0f));

                output[i * 2 + 1] =
                    static_cast<int16_t>(
                        std::lrintf(r * 32767.0f));
            }

            break;
        }

        case SampleFormat::PCM24:
        {
            /*
             * 24-bit PCM is packed as 3 bytes per sample.
             */
            for (UINT32 i = 0; i < framesToWrite; ++i)
            {
                const float l =
                    (std::max)(-1.0f, (std::min)(1.0f, left[i]));

                const float r =
                    (std::max)(-1.0f, (std::min)(1.0f, right[i]));

                const int32_t li =
                    static_cast<int32_t>(
                        std::lrintf(l * 8388607.0f));

                const int32_t ri =
                    static_cast<int32_t>(
                        std::lrintf(r * 8388607.0f));

                BYTE* leftOut =
                    data + (i * 2 + 0) * 3;

                BYTE* rightOut =
                    data + (i * 2 + 1) * 3;

                leftOut[0] =
                    static_cast<BYTE>(li & 0xFF);

                leftOut[1] =
                    static_cast<BYTE>((li >> 8) & 0xFF);

                leftOut[2] =
                    static_cast<BYTE>((li >> 16) & 0xFF);

                rightOut[0] =
                    static_cast<BYTE>(ri & 0xFF);

                rightOut[1] =
                    static_cast<BYTE>((ri >> 8) & 0xFF);

                rightOut[2] =
                    static_cast<BYTE>((ri >> 16) & 0xFF);
            }

            break;
        }

        case SampleFormat::PCM32:
        {
            auto* output =
                reinterpret_cast<int32_t*>(data);

            for (UINT32 i = 0; i < framesToWrite; ++i)
            {
                const float l =
                    (std::max)(-1.0f, (std::min)(1.0f, left[i]));

                const float r =
                    (std::max)(-1.0f, (std::min)(1.0f, right[i]));

                output[i * 2 + 0] =
                    static_cast<int32_t>(
                        std::lrintf(l * 2147483647.0f));

                output[i * 2 + 1] =
                    static_cast<int32_t>(
                        std::lrintf(r * 2147483647.0f));
            }

            break;
        }

        default:
            _renderClient->ReleaseBuffer(
                framesToWrite,
                AUDCLNT_BUFFERFLAGS_SILENT);

            return false;
        }

        hr = _renderClient->ReleaseBuffer(
            framesToWrite,
            0);

        if (FAILED(hr))
        {
            Logger::Log(
                "[Audio] ReleaseBuffer failed: 0x%08X\n",
                static_cast<unsigned>(hr));

            return false;
        }

        return true;
    }


    bool AudioOutput::isInitialized() const
    {
        return _initialized;
    }


    bool AudioOutput::isRunning() const
    {
        return _running;
    }


    double AudioOutput::sampleRate() const
    {
        if (!_format)
            return 0.0;

        return static_cast<double>(
            _format->nSamplesPerSec);
    }


    int32_t AudioOutput::channels() const
    {
        if (!_format)
            return 0;

        return static_cast<int32_t>(
            _format->nChannels);
    }

}