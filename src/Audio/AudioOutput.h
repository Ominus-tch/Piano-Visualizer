#pragma once

#include <cstdint>

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

namespace audio
{

    class AudioOutput
    {
    public:
        AudioOutput();
        ~AudioOutput();

        bool initialize(
            int32_t channels);

        void shutdown();

        bool start();
        void stop();

        bool waitForSpace(int32_t frames);

        bool write(
            const float* left,
            const float* right,
            int32_t numSamples);

        bool isInitialized() const;
        bool isRunning() const;

        double sampleRate() const;
        int32_t channels() const;


    private:
        enum class SampleFormat
        {
            Float32,
            PCM16,
            PCM24,
            PCM32,
            Unknown
        };

        IMMDevice* _device;
        IAudioClient* _audioClient;
        IAudioRenderClient* _renderClient;

        WAVEFORMATEX* _format;

        UINT32 _bufferFrames;

        SampleFormat _sampleFormat;

        bool _initialized;
        bool _running;
        bool _comInitialized;
    };

}