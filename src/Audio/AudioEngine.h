#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <string>


namespace audio
{
    class AudioOutput;
}

namespace vst
{
    class VSTPlugin;
    class VSTAudio;
}

namespace audio
{

    class AudioEngine
    {
    public:
        AudioEngine();

        ~AudioEngine();

        bool initialize();
        bool loadPlugin(const std::string& path);
        void unLoadPlugin();

        bool start();
        void stop();

        void shutdown();

        bool isRunning() const;

        bool noteOn(
            int16_t channel,
            int16_t pitch,
            float velocity);

        bool noteOff(
            int16_t channel,
            int16_t pitch,
            float velocity = 0.0f);

        bool controlChange(
            int16_t channel,
            int16_t controller,
            int16_t value);

        vst::VSTAudio* audio() { return _vstAudio; }
        vst::VSTPlugin* plugin() { return _vstPlugin; }
        AudioOutput* output() { return _output; }

        std::string vstPath() { return _vstPath; }

    private:
        void threadMain();

        vst::VSTAudio* _vstAudio = nullptr;
        vst::VSTPlugin* _vstPlugin = nullptr;
        AudioOutput* _output = nullptr;


        std::thread _thread;

        bool _initialized = false;
        std::string _vstPath = "";
        std::atomic<bool> _running;
        std::atomic<bool> _stopRequested;
    };

}