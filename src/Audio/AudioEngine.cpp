#include "AudioEngine.h"

#include "AudioOutput.h"
#include "VSTPlugin.h"
#include "VSTAudio.h"

#include "../../util/Logger.h"

namespace audio
{

    AudioEngine::AudioEngine() 
        : _running(false),
          _stopRequested(false)
    {
        Logger::Log(
            "[Audio] AudioEngine created\n");
    }


    AudioEngine::~AudioEngine()
    {
        shutdown();

        Logger::Log(
            "[Audio] AudioEngine destroyed\n");
    }

    bool AudioEngine::initialize() 
    {
        _output = new AudioOutput();

        if (!_output->initialize(48000.0, 2))
        {
            delete _output;
            _output = nullptr;
            return false;
        }

        if (!_output->start())
        {
            _output->shutdown();

            delete _output;
            _output = nullptr;

            return false;
        }

        _initialized = true;
        
        Logger::Log(
            "[Audio] AudioEngine initialized\n");

        return true;
    }

    bool AudioEngine::loadPlugin(const std::string& path)
    {
        if (!_initialized)
        {
            Logger::Log("[VST] Cannot load plugin: AudioEngine is not initialized\n");

            return false;
        }

        Logger::Log("[VST] Loading plugin: %s\n", path.c_str());

        _vstPlugin = new vst::VSTPlugin();

        if (!_vstPlugin->load(path, _output->sampleRate()))
        {
            Logger::Log(
                "[VST] Failed to load plugin: %s\n",
                path.c_str());

            delete _vstPlugin;
            _vstPlugin = nullptr;

            return false;
        }

        Logger::Log("[VST] Plugin loaded successfully: %s\n", path.c_str());

        _vstAudio = new vst::VSTAudio(
            _vstPlugin->processor(),
            _output->sampleRate(),
            512);

        return true;
    }

    bool AudioEngine::start()
    {
        if (_running)
            return true;

        if (!_vstAudio)
        {
            Logger::Log(
                "[Audio] Cannot start engine: VSTAudio is null\n");

            return false;
        }

        if (!_output)
        {
            Logger::Log(
                "[Audio] Cannot start engine: AudioOutput is null\n");

            return false;
        }

        if (!_output->isInitialized())
        {
            Logger::Log(
                "[Audio] Cannot start engine: output is not initialized\n");

            return false;
        }

        if (!_output->isRunning())
        {
            Logger::Log(
                "[Audio] Cannot start engine: output is not running\n");

            return false;
        }

        _stopRequested = false;
        _running = true;

        _thread =
            std::thread(
                &AudioEngine::threadMain,
                this);

        Logger::Log(
            "[Audio] Audio engine started\n");

        return true;
    }


    void AudioEngine::stop()
    {
        if (!_running &&
            !_thread.joinable())
        {
            return;
        }

        _stopRequested = true;

        if (_thread.joinable())
            _thread.join();

        _running = false;

        Logger::Log(
            "[Audio] Audio engine stopped\n");
    }

    void AudioEngine::shutdown()
    {
        stop();

        if (_output)
        {
            _output->stop();
            _output->shutdown();
        }

        delete _vstAudio;
        _vstAudio = nullptr;

        if (_vstPlugin)
        {
            _vstPlugin->unload();
            delete _vstPlugin;
            _vstPlugin = nullptr;
        }

        delete _output;
        _output = nullptr;

        _initialized = false;

        Logger::Log(
            "[Audio] AudioEngine shutdown\n");
    }

    bool AudioEngine::isRunning() const
    {
        return _running;
    }

    bool AudioEngine::noteOn(
        int16_t channel,
        int16_t pitch,
        float velocity)
    {
        if (!_vstAudio)
            return false;

        return _vstAudio->noteOn(channel, pitch, velocity);
    }

    bool AudioEngine::noteOff(
        int16_t channel,
        int16_t pitch,
        float velocity)
    {
        if (!_vstAudio)
            return false;

        return _vstAudio->noteOff(channel, pitch, velocity);
    }

    bool AudioEngine::controlChange(
        int16_t channel,
        int16_t controller,
        int16_t value)
    {
        if (!_vstAudio)
            return false;

        return _vstAudio->controlChange(
            channel,
            controller,
            value);
    }


    void AudioEngine::threadMain()
    {
        Logger::Log(
            "[Audio] Audio thread started\n");

        const int32_t blockSize =
            _vstAudio->maxBlockSize();

        while (!_stopRequested)
        {
            /*
             * Wait until WASAPI has enough room for
             * one complete VST processing block.
             */
            if (!_output->waitForSpace(blockSize))
            {
                Logger::Log(
                    "[Audio] Failed waiting for output space\n");

                break;
            }

            if (_stopRequested)
                break;

            /*
             * Generate exactly one VST block.
             */
            if (!_vstAudio->process())
            {
                Logger::Log(
                    "[Audio] VST processing failed\n");

                break;
            }

            /*
             * Send the generated samples to WASAPI.
             */
            if (!_output->write(
                _vstAudio->leftChannel(),
                _vstAudio->rightChannel(),
                _vstAudio->numSamples()))
            {
                Logger::Log(
                    "[Audio] Failed writing VST audio\n");

                break;
            }
        }

        _running = false;

        Logger::Log(
            "[Audio] Audio thread stopped\n");
    }

}