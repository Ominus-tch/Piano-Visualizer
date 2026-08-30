#include "VSTHostApplication.h"

#include "pluginterfaces/base/futils.h"

namespace vst
{
    VSTHostApplication::VSTHostApplication()
        : _refCount(1)
    {
    }


    Steinberg::tresult PLUGIN_API
        VSTHostApplication::getName(
            Steinberg::Vst::String128 name)
    {
        const char16_t hostName[] = u"PianoViz";

        Steinberg::strncpy16(
            name,
            hostName,
            128);

        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTHostApplication::createInstance(
            Steinberg::TUID,
            Steinberg::TUID,
            void** obj)
    {
        if (obj)
            *obj = nullptr;

        return Steinberg::kNoInterface;
    }


    Steinberg::tresult PLUGIN_API
        VSTHostApplication::queryInterface(
            const Steinberg::TUID iid,
            void** obj)
    {
        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::Vst::IHostApplication::iid))
        {
            *obj =
                static_cast<
                Steinberg::Vst::IHostApplication*>(
                    this);
        }
        else if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::FUnknown::iid))
        {
            *obj =
                static_cast<Steinberg::FUnknown*>(
                    this);
        }
        else
        {
            return Steinberg::kNoInterface;
        }

        addRef();

        return Steinberg::kResultOk;
    }


    Steinberg::uint32 PLUGIN_API
        VSTHostApplication::addRef()
    {
        return ++_refCount;
    }


    Steinberg::uint32 PLUGIN_API
        VSTHostApplication::release()
    {
        const auto count = --_refCount;

        if (count == 0)
            delete this;

        return count;
    }
}