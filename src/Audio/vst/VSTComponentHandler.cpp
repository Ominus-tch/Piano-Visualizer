#include "VSTComponentHandler.h"

namespace vst
{
    VSTComponentHandler::VSTComponentHandler()
        : _refCount(1)
    {
    }


    Steinberg::tresult PLUGIN_API
        VSTComponentHandler::beginEdit(
            Steinberg::Vst::ParamID)
    {
        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTComponentHandler::performEdit(
            Steinberg::Vst::ParamID,
            Steinberg::Vst::ParamValue)
    {
        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTComponentHandler::endEdit(
            Steinberg::Vst::ParamID)
    {
        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTComponentHandler::restartComponent(
            Steinberg::int32)
    {
        return Steinberg::kResultOk;
    }


    Steinberg::tresult PLUGIN_API
        VSTComponentHandler::queryInterface(
            const Steinberg::TUID iid,
            void** obj)
    {
        if (!obj)
            return Steinberg::kInvalidArgument;

        *obj = nullptr;

        if (Steinberg::FUnknownPrivate::iidEqual(
            iid,
            Steinberg::Vst::IComponentHandler::iid))
        {
            *obj =
                static_cast<
                Steinberg::Vst::IComponentHandler*>(
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
        VSTComponentHandler::addRef()
    {
        return ++_refCount;
    }


    Steinberg::uint32 PLUGIN_API
        VSTComponentHandler::release()
    {
        const auto count = --_refCount;

        if (count == 0)
            delete this;

        return count;
    }
}