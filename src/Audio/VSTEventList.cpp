#include "VSTEventList.h"

#include "pluginterfaces/base/funknownimpl.h"

#include "../../util/Logger.h"

namespace vst
{

    VSTEventList::VSTEventList()
        : _refCount(1)
    {
    }


    Steinberg::tresult PLUGIN_API VSTEventList::queryInterface(
        const Steinberg::TUID iid,
        void** obj)
    {
        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::Vst::IEventList::iid))
        {
            *obj =
                static_cast<Steinberg::Vst::IEventList*>(this);

            addRef();

            return Steinberg::kResultTrue;
        }

        if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::FUnknown::iid))
        {
            *obj =
                static_cast<Steinberg::FUnknown*>(
                    static_cast<Steinberg::Vst::IEventList*>(this));

            addRef();

            return Steinberg::kResultTrue;
        }

        return Steinberg::kNoInterface;
    }


    Steinberg::uint32 PLUGIN_API VSTEventList::addRef()
    {
        return ++_refCount;
    }


    Steinberg::uint32 PLUGIN_API VSTEventList::release()
    {
        const auto count = --_refCount;

        if (count == 0)
            delete this;

        return count;
    }


    int32_t PLUGIN_API VSTEventList::getEventCount()
    {
        return static_cast<int32_t>(_events.size());
    }


    Steinberg::tresult PLUGIN_API VSTEventList::getEvent(
        int32_t index,
        Steinberg::Vst::Event& e)
    {
        if (index < 0 ||
            index >= static_cast<int32_t>(_events.size()))
        {
            return Steinberg::kResultFalse;
        }

        e = _events[index];

        return Steinberg::kResultTrue;
    }


    Steinberg::tresult PLUGIN_API VSTEventList::addEvent(
        Steinberg::Vst::Event& e)
    {
        _events.push_back(e);

        return Steinberg::kResultTrue;
    }

    bool VSTEventList::addMidiCC(
        int16_t channel,
        int16_t controller,
        int16_t value)
    {
        Logger::Log("CC: %d, %d, %d\n", channel, controller, value);

        if (channel < 0 || channel > 15 ||
            controller < 0 || controller > 127 ||
            value < 0 || value > 127)
        {
            return false;
        }

        std::vector<Steinberg::uint8> midi =
        {
            static_cast<Steinberg::uint8>(0xB0 | channel),
            static_cast<Steinberg::uint8>(controller),
            static_cast<Steinberg::uint8>(value)
        };

        _data.push_back(std::move(midi));

        auto& bytes = _data.back();

        Steinberg::Vst::Event event{};

        event.type = Steinberg::Vst::Event::kLegacyMIDICCOutEvent;
        event.busIndex = 0;
        event.sampleOffset = 0;
        event.flags = Steinberg::Vst::Event::kIsLive;

        event.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
        event.data.size =
            static_cast<Steinberg::uint32>(bytes.size());
        event.data.bytes = bytes.data();

        _events.push_back(event);

        return true;
    }


    void VSTEventList::clear()
    {
        _events.clear();
        _data.clear();
    }

}