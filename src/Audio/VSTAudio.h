#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include "VSTParameterChanges.h"
#include "VSTEventList.h"

namespace vst
{
    class VSTAudio
    {
    public:
        VSTAudio(
            Steinberg::Vst::IAudioProcessor* processor,
            double sampleRate,
            int32_t maxBlockSize);

        ~VSTAudio();

        bool process();

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
            int16_t value,
            int16_t value2 = 0);

        const float* leftChannel() const;
        const float* rightChannel() const;

        int32_t numSamples() const;

        double sampleRate() const;
        int32_t maxBlockSize() const;

    private:
        Steinberg::Vst::ProcessContext _processContext;

        Steinberg::Vst::IAudioProcessor* _processor;

        double _sampleRate;
        int32_t _maxBlockSize;

        std::vector<float> _leftBuffer;
        std::vector<float> _rightBuffer;

        std::vector<float*> _channelBuffers;

        Steinberg::Vst::AudioBusBuffers _outputBus;

        VSTEventList _eventList;

        int32_t _numSamples;

        std::mutex _processMutex;
    };

}