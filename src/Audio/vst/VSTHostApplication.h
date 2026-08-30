#pragma once

#include "pluginterfaces/vst/ivsthostapplication.h"

namespace vst
{
    class VSTHostApplication :
        public Steinberg::Vst::IHostApplication
    {
    public:
        VSTHostApplication();

        ~VSTHostApplication() = default;

        Steinberg::tresult PLUGIN_API getName(
            Steinberg::Vst::String128 name) override;

        Steinberg::tresult PLUGIN_API createInstance(
            Steinberg::TUID cid,
            Steinberg::TUID iid,
            void** obj) override;

        Steinberg::tresult PLUGIN_API queryInterface(
            const Steinberg::TUID iid,
            void** obj) override;

        Steinberg::uint32 PLUGIN_API addRef() override;

        Steinberg::uint32 PLUGIN_API release() override;

    private:
        Steinberg::uint32 _refCount;
    };
}