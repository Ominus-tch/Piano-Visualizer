#pragma once

#include "pluginterfaces/vst/ivsteditcontroller.h"

namespace vst
{
    class VSTComponentHandler :
        public Steinberg::Vst::IComponentHandler
    {
    public:
        VSTComponentHandler();

        ~VSTComponentHandler() = default;

        Steinberg::tresult PLUGIN_API beginEdit(
            Steinberg::Vst::ParamID id) override;

        Steinberg::tresult PLUGIN_API performEdit(
            Steinberg::Vst::ParamID id,
            Steinberg::Vst::ParamValue value) override;

        Steinberg::tresult PLUGIN_API endEdit(
            Steinberg::Vst::ParamID id) override;

        Steinberg::tresult PLUGIN_API restartComponent(
            Steinberg::int32 flags) override;

        Steinberg::tresult PLUGIN_API queryInterface(
            const Steinberg::TUID iid,
            void** obj) override;

        Steinberg::uint32 PLUGIN_API addRef() override;

        Steinberg::uint32 PLUGIN_API release() override;

    private:
        Steinberg::uint32 _refCount;
    };
}