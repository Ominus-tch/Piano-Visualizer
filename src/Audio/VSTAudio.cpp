#include "VSTAudio.h"

#include "../../util/Logger.h"

#include "pluginterfaces/base/funknownimpl.h"

#include <algorithm>
#include <cmath>

namespace vst
{
    VSTAudio::VSTAudio(
        Steinberg::Vst::IAudioProcessor* processor,
        double sampleRate,
        int32_t maxBlockSize)
        : _processor(processor),
        _sampleRate(sampleRate),
        _maxBlockSize(maxBlockSize),
        _numSamples(0)
    {
        Logger::Log("[VST] VSTAudio created\n");

        _leftBuffer.resize(maxBlockSize);
        _rightBuffer.resize(maxBlockSize);

        _channelBuffers.resize(2);

        _channelBuffers[0] = _leftBuffer.data();
        _channelBuffers[1] = _rightBuffer.data();

        _outputBus = {};
        _outputBus.numChannels = 2;
        _outputBus.silenceFlags = 0;
        _outputBus.channelBuffers32 = _channelBuffers.data();

        _processContext = {};
        _processContext.sampleRate = sampleRate;
        _processContext.projectTimeSamples = 0.0;
        _processContext.projectTimeMusic = 0.0;
        _processContext.tempo = 120.0;
        _processContext.timeSigNumerator = 4;
        _processContext.timeSigDenominator = 4;
        _processContext.state =
            Steinberg::Vst::ProcessContext::kPlaying;
    }


    VSTAudio::~VSTAudio()
    {
        Logger::Log("[VST] VSTAudio destroyed\n");
    }


    bool VSTAudio::process()
    {
        std::lock_guard<std::mutex> lock(_processMutex);

        if (!_processor)
        {
            Logger::Log(
                "[VST] Cannot process: processor is null\n");

            return false;
        }

        _numSamples = _maxBlockSize;

        std::fill(
            _leftBuffer.begin(),
            _leftBuffer.end(),
            0.0f);

        std::fill(
            _rightBuffer.begin(),
            _rightBuffer.end(),
            0.0f);

        _outputBus.silenceFlags = 0;

        Steinberg::Vst::ProcessData data{};

        data.processMode =
            Steinberg::Vst::kRealtime;

        data.symbolicSampleSize =
            Steinberg::Vst::kSample32;

        data.numSamples =
            _numSamples;

        data.numInputs = 0;
        data.inputs = nullptr;

        data.numOutputs = 1;
        data.outputs = &_outputBus;

        data.inputEvents = &_eventList;
        data.outputEvents = nullptr;

        data.processContext =
            &_processContext;

        const auto result =
            _processor->process(data);

        _processContext.projectTimeSamples +=
            _numSamples;

        _processContext.projectTimeMusic +=
            static_cast<double>(_numSamples) *
            120.0 /
            (60.0 * _sampleRate);

        _eventList.clear();

        if (result != Steinberg::kResultOk)
        {
            Logger::Log(
                "[VST] process() failed\n");

            return false;
        }

        return true;
    }


    bool VSTAudio::noteOn(
        int16_t channel,
        int16_t pitch,
        float velocity)
    {
        std::lock_guard<std::mutex> lock(_processMutex);

        if (!_processor)
        {
            Logger::Log(
                "[VST] Cannot send note on: processor is null\n");

            return false;
        }

        if (channel < 0 || channel > 15)
        {
            Logger::Log(
                "[VST] Invalid MIDI channel: %d\n",
                channel);

            return false;
        }

        if (pitch < 0 || pitch > 127)
        {
            Logger::Log(
                "[VST] Invalid MIDI pitch: %d\n",
                pitch);

            return false;
        }

        if (velocity < 0.0f || velocity > 1.0f)
        {
            Logger::Log(
                "[VST] Invalid MIDI velocity: %f\n",
                velocity);

            return false;
        }

        Steinberg::Vst::Event event{};

        event.type =
            Steinberg::Vst::Event::kNoteOnEvent;

        event.busIndex = 0;
        event.sampleOffset = 0;
        event.flags =
            Steinberg::Vst::Event::kIsLive;

        event.noteOn.channel = channel;
        event.noteOn.pitch = pitch;
        event.noteOn.tuning = 0.0f;
        event.noteOn.velocity = velocity;
        event.noteOn.length = 0;
        event.noteOn.noteId = pitch;

        if (_eventList.addEvent(event) !=
            Steinberg::kResultTrue)
        {
            Logger::Log(
                "[VST] Failed to queue note on\n");

            return false;
        }

        Logger::Log(
            "[VST] Queued Note On: channel=%d pitch=%d velocity=%f\n",
            channel,
            pitch,
            velocity);

        return true;
    }


    bool VSTAudio::noteOff(
        int16_t channel,
        int16_t pitch,
        float velocity)
    {
        std::lock_guard<std::mutex> lock(_processMutex);

        if (!_processor)
        {
            Logger::Log(
                "[VST] Cannot send note off: processor is null\n");

            return false;
        }

        if (channel < 0 || channel > 15)
        {
            Logger::Log(
                "[VST] Invalid MIDI channel: %d\n",
                channel);

            return false;
        }

        if (pitch < 0 || pitch > 127)
        {
            Logger::Log(
                "[VST] Invalid MIDI pitch: %d\n",
                pitch);

            return false;
        }

        if (velocity < 0.0f || velocity > 1.0f)
        {
            Logger::Log(
                "[VST] Invalid MIDI velocity: %f\n",
                velocity);

            return false;
        }

        Steinberg::Vst::Event event{};

        event.type =
            Steinberg::Vst::Event::kNoteOffEvent;

        event.busIndex = 0;
        event.sampleOffset = 0;
        event.flags =
            Steinberg::Vst::Event::kIsLive;

        event.noteOff.channel = channel;
        event.noteOff.pitch = pitch;
        event.noteOff.tuning = 0.0f;
        event.noteOff.velocity = velocity;
        event.noteOff.noteId = pitch;

        if (_eventList.addEvent(event) !=
            Steinberg::kResultTrue)
        {
            Logger::Log(
                "[VST] Failed to queue note off\n");

            return false;
        }

        Logger::Log(
            "[VST] Queued Note Off: channel=%d pitch=%d velocity=%f\n",
            channel,
            pitch,
            velocity);

        return true;
    }

    bool VSTAudio::controlChange(
        int16_t channel,
        int16_t controller,
        int16_t value,
        int16_t value2)
    {
        if (!_processor)
            return false;

        if (channel < 0 || channel > 15 ||
            controller < 0 || controller > 127 ||
            value < 0 || value > 127)
        {
            return false;
        }

        Steinberg::Vst::Event event{};

        event.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
        event.busIndex = 0;
        event.sampleOffset = 0;
        event.flags = Steinberg::Vst::Event::kIsLive;

        event.midiCCOut.channel = static_cast<Steinberg::int8>(channel);
        event.midiCCOut.controlNumber = static_cast<Steinberg::uint8>(controller);
        event.midiCCOut.value = static_cast<Steinberg::int8>(value);
        event.midiCCOut.value2 = static_cast<Steinberg::int8>(value2);

        if (_eventList.addEvent(event) !=
            Steinberg::kResultTrue)
        {
            Logger::Log(
                "[VST] Failed to queue control change\n");

            return false;
        }

        return true;
    }

    const float* VSTAudio::leftChannel() const
    {
        return _leftBuffer.data();
    }


    const float* VSTAudio::rightChannel() const
    {
        return _rightBuffer.data();
    }


    int32_t VSTAudio::numSamples() const
    {
        return _numSamples;
    }


    double VSTAudio::sampleRate() const
    {
        return _sampleRate;
    }


    int32_t VSTAudio::maxBlockSize() const
    {
        return _maxBlockSize;
    }

}