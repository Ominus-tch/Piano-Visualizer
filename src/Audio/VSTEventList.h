#pragma once

#include <cstdint>
#include <vector>

#include "pluginterfaces/vst/ivstevents.h"

namespace vst
{

    class VSTEventList : public Steinberg::Vst::IEventList
    {
    public:
        VSTEventList();

        Steinberg::tresult PLUGIN_API queryInterface(
            const Steinberg::TUID iid,
            void** obj) override;

        Steinberg::uint32 PLUGIN_API addRef() override;
        Steinberg::uint32 PLUGIN_API release() override;

        int32_t PLUGIN_API getEventCount() override;

        Steinberg::tresult PLUGIN_API getEvent(
            int32_t index,
            Steinberg::Vst::Event& e) override;

        Steinberg::tresult PLUGIN_API addEvent(
            Steinberg::Vst::Event& e) override;

        bool addMidiCC(
            int16_t channel,
            int16_t controller,
            int16_t value);

        void clear();

    private:
        Steinberg::uint32 _refCount;
        std::vector<Steinberg::Vst::Event> _events;
        std::vector<std::vector<Steinberg::uint8>> _data;
    };

}