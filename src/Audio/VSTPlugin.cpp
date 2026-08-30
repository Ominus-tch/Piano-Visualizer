#include "VSTPlugin.h"

#include <Windows.h>

#include "../../util/Logger.h"
#include "VSTAudio.h"

#include "pluginterfaces/vst/vsttypes.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstmessage.h"

namespace vst
{
    static LRESULT CALLBACK vstEditorWindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam)
    {
        auto* plugin =
            reinterpret_cast<VSTPlugin*>(
                GetWindowLongPtr(
                    hwnd,
                    GWLP_USERDATA));

        switch (msg)
        {
        case WM_SIZE:
        {
            if (plugin &&
                plugin->editor())
            {
                const int width =
                    LOWORD(lParam);

                const int height =
                    HIWORD(lParam);

                if (width > 0 &&
                    height > 0)
                {
                    Steinberg::ViewRect rect(
                        0,
                        0,
                        width,
                        height);

                    plugin->editor()->onSize(
                        &rect);
                }
            }

            break;
        }

        case WM_NCDESTROY:
        {
            SetWindowLongPtr(
                hwnd,
                GWLP_USERDATA,
                0);

            break;
        }
        }

        return DefWindowProc(
            hwnd,
            msg,
            wParam,
            lParam);
    }


    VSTPlugin::VSTPlugin()
    {
        Logger::Log(
            "[VST] VSTPlugin created\n");
    }


    VSTPlugin::~VSTPlugin()
    {
        unload();
    }


    bool VSTPlugin::load(
        const std::string& path,
        double sampleRate)
    {
        unload();

        Logger::Log(
            "[VST] Loading plugin: %s\n",
            path.c_str());

        std::string error;

        _module =
            VST3::Hosting::Module::create(
                path,
                error);

        if (!_module)
        {
            Logger::Log(
                "[VST] Failed to create module: %s\n",
                error.c_str());

            return false;
        }

        Logger::Log(
            "[VST] Module loaded successfully\n");

        const auto& factory =
            _module->getFactory();

        const auto classes =
            factory.classInfos();

        Logger::Log(
            "[VST] Factory contains %zu classes\n",
            classes.size());

        for (const auto& classInfo : classes)
        {
            Logger::Log(
                "[VST] Class: %s | Category: %s | CID: %s\n",
                classInfo.name().data(),
                classInfo.category().data(),
                classInfo.ID().toString().c_str());
        }

        for (const auto& classInfo : classes)
        {
            Logger::Log(
                "[VST] Class: %s | Category: %s\n",
                classInfo.name().data(),
                classInfo.category().data());

            if (classInfo.category() !=
                "Audio Module Class")
            {
                continue;
            }

            Logger::Log(
                "[VST] Found audio effect: %s\n",
                classInfo.name().data());

            _provider =
                Steinberg::owned(
                    new Steinberg::Vst::PlugProvider(
                        factory,
                        classInfo,
                        true));

            if (!_provider)
            {
                Logger::Log(
                    "[VST] Failed to create PlugProvider\n");

                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] PlugProvider created\n");

            if (!_provider->initialize())
            {
                Logger::Log(
                    "[VST] Failed to initialize PlugProvider\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            auto* component =
                _provider->getComponent();

            auto* controller =
                _provider->getController();

            Logger::Log(
                "[VST] Component pointer: %p\n",
                component);

            Logger::Log(
                "[VST] Controller pointer: %p\n",
                controller);

            if (!component)
            {
                Logger::Log(
                    "[VST] Plugin has no component\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            if (!controller)
            {
                Logger::Log(
                    "[VST] Plugin has no controller\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            /*
             * The PlugProvider has already created the correct
             * controller for the component.
             *
             * Give that controller our host-side
             * IComponentHandler.
             */
            _componentHandler =
                new VSTComponentHandler();

            Logger::Log(
                "[VST] Created ComponentHandler: %p\n",
                _componentHandler);

            const auto handlerResult =
                controller->setComponentHandler(
                    _componentHandler);

            Logger::Log(
                "[VST] setComponentHandler result: %d\n",
                handlerResult);

            if (handlerResult !=
                Steinberg::kResultOk)
            {
                _componentHandler->release();
                _componentHandler = nullptr;

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Component and controller are valid\n");

            auto processor =
                Steinberg::FUnknownPtr<
                Steinberg::Vst::IAudioProcessor>(
                    component);

            if (!processor)
            {
                Logger::Log(
                    "[VST] Plugin does not implement "
                    "IAudioProcessor\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Controller parameter count AFTER handler: %d\n",
                controller->getParameterCount());

            Logger::Log(
                "[VST] Plugin initialized successfully: %s\n",
                classInfo.name().data());

            Steinberg::Vst::ProcessSetup setup{};

            setup.processMode =
                Steinberg::Vst::kRealtime;

            setup.symbolicSampleSize =
                Steinberg::Vst::kSample32;

            setup.sampleRate =
                sampleRate;

            setup.maxSamplesPerBlock =
                512;

            Logger::Log(
                "[VST] Setting up processing: "
                "sampleRate=%f, maxBlockSize=%d\n",
                setup.sampleRate,
                setup.maxSamplesPerBlock);

            if (processor->setupProcessing(setup) !=
                Steinberg::kResultTrue)
            {
                Logger::Log(
                    "[VST] setupProcessing() failed\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] setupProcessing() succeeded\n");

            const int32_t outputCount =
                component->getBusCount(
                    Steinberg::Vst::kAudio,
                    Steinberg::Vst::kOutput);

            Logger::Log(
                "[VST] Audio output buses: %d\n",
                outputCount);

            if (outputCount <= 0)
            {
                Logger::Log(
                    "[VST] Plugin has no audio output buses\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Steinberg::Vst::BusInfo outputBusInfo{};

            if (component->getBusInfo(
                Steinberg::Vst::kAudio,
                Steinberg::Vst::kOutput,
                0,
                outputBusInfo) !=
                Steinberg::kResultTrue)
            {
                Logger::Log(
                    "[VST] Failed to get output bus information\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Output bus 0: name='%s', "
                "channels=%d, flags=0x%X\n",
                outputBusInfo.name,
                outputBusInfo.channelCount,
                outputBusInfo.flags);

            if (component->activateBus(
                Steinberg::Vst::kAudio,
                Steinberg::Vst::kOutput,
                0,
                1) !=
                Steinberg::kResultTrue)
            {
                Logger::Log(
                    "[VST] Failed to activate output bus\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Stereo output bus activated\n");

            Steinberg::Vst::SpeakerArrangement
                outputArrangement =
                Steinberg::Vst::SpeakerArr::kStereo;

            if (processor->setBusArrangements(
                nullptr,
                0,
                &outputArrangement,
                1) !=
                Steinberg::kResultTrue)
            {
                Logger::Log(
                    "[VST] Failed to set stereo "
                    "output arrangement\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Stereo output arrangement configured\n");

            const int32_t eventInputCount =
                component->getBusCount(
                    Steinberg::Vst::kEvent,
                    Steinberg::Vst::kInput);

            const int32_t eventOutputCount =
                component->getBusCount(
                    Steinberg::Vst::kEvent,
                    Steinberg::Vst::kOutput);

            Logger::Log(
                "[VST] Event input buses: %d\n",
                eventInputCount);

            Logger::Log(
                "[VST] Event output buses: %d\n",
                eventOutputCount);

            for (int32_t i = 0;
                i < eventInputCount;
                ++i)
            {
                Steinberg::Vst::BusInfo busInfo{};

                if (component->getBusInfo(
                    Steinberg::Vst::kEvent,
                    Steinberg::Vst::kInput,
                    i,
                    busInfo) ==
                    Steinberg::kResultTrue)
                {
                    Logger::Log(
                        "[VST] Event input bus %d: "
                        "name='%s', channels=%d, flags=0x%X\n",
                        i,
                        busInfo.name,
                        busInfo.channelCount,
                        busInfo.flags);
                }
            }

            if (component->setActive(1) !=
                Steinberg::kResultTrue)
            {
                Logger::Log(
                    "[VST] setActive() failed\n");

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Component is now active\n");

            if (processor->setProcessing(1) !=
                Steinberg::kResultTrue)
            {
                Logger::Log(
                    "[VST] setProcessing(true) failed\n");

                component->setActive(0);

                _provider.reset();
                _module.reset();

                return false;
            }

            Logger::Log(
                "[VST] Audio processing state enabled\n");

            _path = path;

            return true;
        }

        Logger::Log(
            "[VST] No VST3 audio effect found in module\n");

        _module.reset();

        return false;
    }


    void VSTPlugin::unload()
    {
        destroyEditor();

        if (_provider)
        {
            auto* component =
                _provider->getComponent();

            auto processor =
                component
                ? Steinberg::FUnknownPtr<
                Steinberg::Vst::IAudioProcessor>(
                    component)
                : nullptr;

            if (processor)
            {
                processor->setProcessing(0);
            }

            if (component)
            {
                Logger::Log(
                    "[VST] Deactivating component\n");

                component->setActive(0);
            }

            Logger::Log(
                "[VST] Releasing plugin\n");

            _provider.reset();
        }

        if (_componentHandler)
        {
            Logger::Log(
                "[VST] Releasing ComponentHandler\n");

            _componentHandler->release();

            _componentHandler = nullptr;
        }

        if (_module)
        {
            Logger::Log(
                "[VST] Unloading module\n");

            _module.reset();
        }

        _path.clear();
    }


    bool VSTPlugin::isLoaded() const
    {
        return _module != nullptr &&
            _provider != nullptr;
    }


    const std::string& VSTPlugin::path() const
    {
        return _path;
    }


    Steinberg::Vst::IComponent*
        VSTPlugin::component() const
    {
        if (!_provider)
            return nullptr;

        return _provider->getComponent();
    }


    Steinberg::Vst::IEditController*
        VSTPlugin::controller() const
    {
        if (!_provider)
            return nullptr;

        return _provider->getController();
    }


    Steinberg::Vst::IAudioProcessor*
        VSTPlugin::processor() const
    {
        if (!_provider)
            return nullptr;

        auto* component =
            _provider->getComponent();

        if (!component)
            return nullptr;

        return Steinberg::FUnknownPtr<
            Steinberg::Vst::IAudioProcessor>(
                component);
    }


    bool VSTPlugin::createEditor(
        HINSTANCE instance)
    {
        if (!_provider)
        {
            Logger::Log(
                "[VST] Cannot create editor: "
                "plugin not loaded\n");

            return false;
        }

        if (_editor)
        {
            Logger::Log(
                "[VST] Editor already exists\n");

            if (_editorWindow)
            {
                ShowWindow(
                    _editorWindow,
                    SW_SHOW);

                SetForegroundWindow(
                    _editorWindow);
            }

            return true;
        }

        auto* component =
            _provider->getComponent();

        auto* controller =
            _provider->getController();

        Logger::Log(
            "[VST] Component pointer: %p\n",
            component);

        Logger::Log(
            "[VST] Controller pointer: %p\n",
            controller);

        if (!component)
        {
            Logger::Log(
                "[VST] Component is null\n");

            return false;
        }

        if (!controller)
        {
            Logger::Log(
                "[VST] Controller is null\n");

            return false;
        }

        /*
         * This is the SAME controller that was returned by
         * PlugProvider during load().
         *
         * Do not create another controller.
         * Do not query another controller CID.
         */
        Logger::Log(
            "[VST] Controller parameter count: %d\n",
            controller->getParameterCount());

        auto editController2 =
            Steinberg::FUnknownPtr<
            Steinberg::Vst::IEditController2>(
                controller);

        Logger::Log(
            "[VST] IEditController2: %s\n",
            editController2
            ? "YES"
            : "NO");

        Logger::Log(
            "[VST] Host ComponentHandler: %p\n",
            _componentHandler);

        /*
         * Print the actual virtual function table so we can
         * inspect exactly what createView() is dispatching to.
         */
        auto** vtable = *reinterpret_cast<void***>(controller);

        Logger::Log(
            "[VST] Controller vtable: %p\n",
            vtable);

        Logger::Log(
            "[VST] createView slot address: %p\n",
            vtable[17]);

        Logger::Log(
            "[VST] Requesting editor view type: %s\n",
            Steinberg::Vst::ViewType::kEditor);

        Steinberg::Vst::IEditController* controller2 =
            _provider->getController();

        Logger::Log(
            "[VST] controller == provider controller: %s\n",
            controller == controller2 ? "YES" : "NO");

        Logger::Log(
            "[VST] controller object: %p\n",
            controller);

        Logger::Log(
            "[VST] controller vtable: %p\n",
            *reinterpret_cast<void***>(controller));

        auto* view =
            controller->createView(
                Steinberg::Vst::ViewType::kEditor);

        Logger::Log(
            "[VST] createView returned: %p\n",
            view);

        if (!view)
        {
            Logger::Log(
                "[VST] Plugin returned no editor view\n");

            return false;
        }

        _editor =
            Steinberg::IPtr<
            Steinberg::IPlugView>(
                view);

        if (!_editor)
        {
            Logger::Log(
                "[VST] Failed to store editor\n");

            view->release();

            return false;
        }

        if (_editor->isPlatformTypeSupported(
            Steinberg::kPlatformTypeHWND) !=
            Steinberg::kResultTrue)
        {
            Logger::Log(
                "[VST] Editor does not support HWND\n");

            _editor = nullptr;

            return false;
        }

        Steinberg::ViewRect rect{};

        if (_editor->getSize(&rect) !=
            Steinberg::kResultTrue)
        {
            Logger::Log(
                "[VST] Failed to get editor size\n");

            _editor = nullptr;

            return false;
        }

        const int width =
            rect.getWidth();

        const int height =
            rect.getHeight();

        Logger::Log(
            "[VST] Editor size: %d x %d\n",
            width,
            height);

        const DWORD style =
            WS_OVERLAPPEDWINDOW;

        const DWORD exStyle =
            0;

        RECT windowRect{
            0,
            0,
            width,
            height
        };

        if (!AdjustWindowRectEx(
            &windowRect,
            style,
            FALSE,
            exStyle))
        {
            Logger::Log(
                "[VST] Failed to calculate "
                "editor window size\n");

            _editor = nullptr;

            return false;
        }

        const int windowWidth =
            windowRect.right -
            windowRect.left;

        const int windowHeight =
            windowRect.bottom -
            windowRect.top;

        static bool windowClassRegistered =
            false;

        if (!windowClassRegistered)
        {
            WNDCLASSA wc{};

            wc.style =
                CS_HREDRAW |
                CS_VREDRAW;

            wc.lpfnWndProc =
                vstEditorWindowProc;

            wc.hInstance =
                instance;

            wc.hCursor =
                LoadCursor(
                    nullptr,
                    IDC_ARROW);

            wc.lpszClassName =
                "VST Editor Host";

            if (!RegisterClassA(&wc))
            {
                if (GetLastError() !=
                    ERROR_CLASS_ALREADY_EXISTS)
                {
                    Logger::Log(
                        "[VST] Failed to register "
                        "editor window class\n");

                    _editor = nullptr;

                    return false;
                }
            }

            windowClassRegistered =
                true;
        }

        _editorWindow =
            CreateWindowExA(
                exStyle,
                "VST Editor Host",
                "VST Editor",
                style,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                windowWidth,
                windowHeight,
                nullptr,
                nullptr,
                instance,
                nullptr);

        if (!_editorWindow)
        {
            Logger::Log(
                "[VST] Failed to create editor window\n");

            _editor = nullptr;

            return false;
        }

        SetWindowLongPtr(
            _editorWindow,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(
                this));

        const auto result =
            _editor->attached(
                _editorWindow,
                Steinberg::kPlatformTypeHWND);

        if (result !=
            Steinberg::kResultOk)
        {
            Logger::Log(
                "[VST] Failed to attach editor: result=%d\n",
                result);

            SetWindowLongPtr(
                _editorWindow,
                GWLP_USERDATA,
                0);

            DestroyWindow(
                _editorWindow);

            _editorWindow =
                nullptr;

            _editor =
                nullptr;

            return false;
        }

        ShowWindow(
            _editorWindow,
            SW_SHOW);

        UpdateWindow(
            _editorWindow);

        Logger::Log(
            "[VST] Editor attached successfully\n");

        return true;
    }


    void VSTPlugin::destroyEditor()
    {
        if (!_editor &&
            !_editorWindow)
        {
            return;
        }

        Logger::Log(
            "[VST] Destroying editor\n");

        if (_editor)
        {
            _editor->removed();

            _editor = nullptr;
        }

        if (_editorWindow)
        {
            SetWindowLongPtr(
                _editorWindow,
                GWLP_USERDATA,
                0);

            DestroyWindow(
                _editorWindow);

            _editorWindow =
                nullptr;
        }

        Logger::Log(
            "[VST] Editor destroyed\n");
    }


    bool VSTPlugin::hasEditor() const
    {
        return _editor != nullptr &&
            _editorWindow != nullptr;
    }


    HWND VSTPlugin::editorWindow() const
    {
        return _editorWindow;
    }


    Steinberg::IPlugView*
        VSTPlugin::editor() const
    {
        return _editor;
    }


    void VSTPlugin::resizeEditor(
        int x,
        int y,
        int width,
        int height)
    {
        if (!_editor ||
            !_editorWindow)
        {
            return;
        }

        DWORD style =
            static_cast<DWORD>(
                GetWindowLongPtr(
                    _editorWindow,
                    GWL_STYLE));

        DWORD exStyle =
            static_cast<DWORD>(
                GetWindowLongPtr(
                    _editorWindow,
                    GWL_EXSTYLE));

        RECT rect{
            0,
            0,
            width,
            height
        };

        if (!AdjustWindowRectEx(
            &rect,
            style,
            FALSE,
            exStyle))
        {
            return;
        }

        const int windowWidth =
            rect.right -
            rect.left;

        const int windowHeight =
            rect.bottom -
            rect.top;

        SetWindowPos(
            _editorWindow,
            nullptr,
            x,
            y,
            windowWidth,
            windowHeight,
            SWP_NOACTIVATE |
            SWP_NOZORDER);
    }
}