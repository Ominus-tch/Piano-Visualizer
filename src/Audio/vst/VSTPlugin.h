#pragma once

#include <Windows.h>
#include <string>

#include "public.sdk/source/vst/hosting/module.h"

#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcomponent.h"

#include "pluginterfaces/gui/iplugview.h"

#include "VSTHostApplication.h"
#include "VSTComponentHandler.h"

namespace vst
{
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

        bool saveState(
            const std::string& path);

        bool loadState(
            const std::string& path);

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

        const std::string& editorName() const { return _editorName; }

        Steinberg::IPlugView*
            editor() const;

        void resizeEditor(
            int x,
            int y,
            int width,
            int height);

        void onEditorWindowDestroyed();

    private:
        bool _loaded = false;

        std::string _path;

        VST3::Hosting::Module::Ptr _module;

        bool _singleComponent = false;
        Steinberg::IPtr<
            Steinberg::Vst::IComponent>
            _component;

        Steinberg::IPtr<
            Steinberg::Vst::IEditController>
            _controller;

        Steinberg::IPtr<
            Steinberg::Vst::IAudioProcessor>
            _processor;

        Steinberg::IPtr<
            Steinberg::Vst::IConnectionPoint>
            _componentConnection;

        Steinberg::IPtr<
            Steinberg::Vst::IConnectionPoint>
            _controllerConnection;

        VSTHostApplication*
            _hostApplication = nullptr;

        VSTComponentHandler*
            _componentHandler = nullptr;

        Steinberg::IPtr<
            Steinberg::IPlugView>
            _editor;

        HWND _editorWindow{
            nullptr
        };

        std::string _editorName = "VST Editor";
        bool _destroyingEditor = false;
    };
}