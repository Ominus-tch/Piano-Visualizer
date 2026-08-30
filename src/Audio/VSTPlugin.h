#pragma once

#include <Windows.h>
#include <memory>
#include <string>
#include <atomic>

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"

#include "pluginterfaces/gui/iplugview.h"

namespace vst
{
    class VSTComponentHandler :
        public Steinberg::Vst::IComponentHandler
    {
    public:
        Steinberg::tresult PLUGIN_API beginEdit(
            Steinberg::Vst::ParamID) override
        {
            return Steinberg::kResultOk;
        }

        Steinberg::tresult PLUGIN_API performEdit(
            Steinberg::Vst::ParamID,
            Steinberg::Vst::ParamValue) override
        {
            return Steinberg::kResultOk;
        }

        Steinberg::tresult PLUGIN_API endEdit(
            Steinberg::Vst::ParamID) override
        {
            return Steinberg::kResultOk;
        }

        Steinberg::tresult PLUGIN_API restartComponent(
            Steinberg::int32) override
        {
            return Steinberg::kResultOk;
        }

        Steinberg::tresult PLUGIN_API queryInterface(
            const Steinberg::TUID _iid,
            void** obj) override
        {
            if (!obj)
                return Steinberg::kInvalidArgument;

            *obj = nullptr;

            if (Steinberg::FUnknownPrivate::iidEqual(
                _iid,
                Steinberg::FUnknown::iid))
            {
                *obj =
                    static_cast<
                    Steinberg::Vst::IComponentHandler*>(
                        this);

                addRef();

                return Steinberg::kResultOk;
            }

            if (Steinberg::FUnknownPrivate::iidEqual(
                _iid,
                Steinberg::Vst::IComponentHandler::iid))
            {
                *obj =
                    static_cast<
                    Steinberg::Vst::IComponentHandler*>(
                        this);

                addRef();

                return Steinberg::kResultOk;
            }

            return Steinberg::kNoInterface;
        }

        Steinberg::uint32 PLUGIN_API addRef() override
        {
            return ++refCount;
        }

        Steinberg::uint32 PLUGIN_API release() override
        {
            auto count = --refCount;

            if (count == 0)
                delete this;

            return count;
        }

    private:
        std::atomic<Steinberg::uint32> refCount{ 1 };
    };


    class VSTPlugin
    {
    public:
        VSTPlugin();
        ~VSTPlugin();

        bool load(
            const std::string& path,
            double sampleRate = 44100.0);

        void unload();

        bool isLoaded() const;

        const std::string& path() const;

        Steinberg::Vst::IComponent*
            component() const;

        Steinberg::Vst::IEditController*
            controller() const;

        Steinberg::Vst::IAudioProcessor*
            processor() const;

        bool createEditor(
            HINSTANCE instance);

        void destroyEditor();

        bool hasEditor() const;

        HWND editorWindow() const;

        Steinberg::IPlugView*
            editor() const;

        void resizeEditor(
            int x,
            int y,
            int width,
            int height);

    private:
        std::string _path;

        VST3::Hosting::Module::Ptr _module;

        Steinberg::IPtr<
            Steinberg::Vst::PlugProvider>
            _provider;

        VSTComponentHandler*
            _componentHandler{
                nullptr
        };

        Steinberg::IPtr<
            Steinberg::IPlugView>
            _editor;

        HWND _editorWindow{
            nullptr
        };
    };
}