#include "VSTPlugin.h"
#include "VSTAudio.h"

#include <Windows.h>

#include "../../../util/Logger.h"

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

        /*
         * Create the host objects first.
         *
         * These objects are passed to the plugin's
         * component and controller during initialization.
         */
        _hostApplication =
            new VSTHostApplication();

        _componentHandler =
            new VSTComponentHandler();

        if (!_hostApplication ||
            !_componentHandler)
        {
            Logger::Log(
                "[VST] Failed to create host interfaces\n");

            unload();

            return false;
        }

        const auto& factory =
            _module->getFactory();

        const auto classes =
            factory.classInfos();

        for (const auto& classInfo : classes)
        {
            if (classInfo.category() !=
                "Audio Module Class")
            {
                continue;
            }
            
            _editorName = classInfo.name();

            /*
             * --------------------------------------------------------
             * Create component
             * --------------------------------------------------------
             */

            _component =
                factory.createInstance<
                Steinberg::Vst::IComponent>(
                    classInfo.ID());

            if (!_component)
            {
                Logger::Log(
                    "[VST] Failed to create IComponent\n");

                unload();

                return false;
            }

            /*
             * The component implements IPluginBase, which provides
             * initialize().
             */
            auto* componentPluginBase =
                static_cast<
                Steinberg::IPluginBase*>(
                    _component.get());

            if (!componentPluginBase)
            {
                Logger::Log(
                    "[VST] Failed to obtain IPluginBase from component\n");

                unload();

                return false;
            }

            auto result =
                componentPluginBase->initialize(
                    _hostApplication);

            if (result !=
                Steinberg::kResultOk)
            {
                Logger::Log(
                    "[VST] Component initialize failed: result=%d\n",
                    result);

                unload();

                return false;
            }

            /*
             * --------------------------------------------------------
             * Get IAudioProcessor
             * --------------------------------------------------------
             */

            Steinberg::Vst::IAudioProcessor*
                processor = nullptr;

            result =
                _component->queryInterface(
                    Steinberg::Vst::IAudioProcessor::iid,
                    reinterpret_cast<void**>(
                        &processor));

            if (result != Steinberg::kResultOk ||
                !processor)
            {
                Logger::Log(
                    "[VST] Failed to obtain IAudioProcessor\n");

                unload();

                return false;
            }

            _processor = processor;

            /*
             * --------------------------------------------------------
             * Get controller
             * --------------------------------------------------------
             *
             * A VST3 can either:
             *
             * 1. Have IEditController implemented by the same
             *    component object.
             *
             * 2. Have a separate controller object.
             */

            _singleComponent =
                false;

            Steinberg::Vst::IEditController*
                controllerFromComponent =
                nullptr;

            result =
                _component->queryInterface(
                    Steinberg::Vst::IEditController::iid,
                    reinterpret_cast<void**>(
                        &controllerFromComponent));

            if (result ==
                Steinberg::kResultOk &&
                controllerFromComponent)
            {
                /*
                 * The component itself is also the controller.
                 */
                _controller =
                    controllerFromComponent;

                _singleComponent = true;
            }
            else
            {
                /*
                 * The controller is a separate component.
                 * Ask the audio component which controller CID
                 * belongs to it.
                 */
                Steinberg::TUID controllerCID{};

                result =
                    _component->getControllerClassId(
                        controllerCID);

                if (result !=
                    Steinberg::kResultOk)
                {
                    Logger::Log(
                        "[VST] Failed to get controller class ID: result=%d\n",
                        result);

                    unload();

                    return false;
                }

                /*
                 * Create the controller using the factory.
                 */
                _controller =
                    factory.createInstance<
                    Steinberg::Vst::IEditController>(
                        VST3::UID(
                            controllerCID));

                if (!_controller)
                {
                    Logger::Log(
                        "[VST] Failed to create IEditController\n");

                    unload();

                    return false;
                }

                /*
                 * Initialize the separate controller.
                 */
                auto* controllerPluginBase =
                    static_cast<
                    Steinberg::IPluginBase*>(
                        _controller.get());

                if (!controllerPluginBase)
                {
                    Logger::Log(
                        "[VST] Failed to obtain IPluginBase from controller\n");

                    unload();

                    return false;
                }

                result =
                    controllerPluginBase->initialize(
                        _hostApplication);

                if (result !=
                    Steinberg::kResultOk)
                {
                    Logger::Log(
                        "[VST] Controller initialize failed: result=%d\n",
                        result);

                    unload();

                    return false;
                }
            }

            /*
             * --------------------------------------------------------
             * Give controller its ComponentHandler
             * --------------------------------------------------------
             */

            if (_controller)
            {
                result =
                    _controller->setComponentHandler(
                        _componentHandler);

                if (result !=
                    Steinberg::kResultOk)
                {
                    Logger::Log(
                        "[VST] setComponentHandler failed: result=%d\n",
                        result);

                    unload();

                    return false;
                }
            }

            /*
             * --------------------------------------------------------
             * Connect component and controller
             * --------------------------------------------------------
             *
             * A single-component plugin does not need this because
             * component and controller are the same object.
             */

            if (!_singleComponent)
            {
                Steinberg::Vst::IConnectionPoint*
                    componentConnection =
                    nullptr;

                Steinberg::Vst::IConnectionPoint*
                    controllerConnection =
                    nullptr;

                result =
                    _component->queryInterface(
                        Steinberg::Vst::IConnectionPoint::iid,
                        reinterpret_cast<void**>(
                            &componentConnection));

                if (result !=
                    Steinberg::kResultOk ||
                    !componentConnection)
                {
                    Logger::Log(
                        "[VST] Component does not provide IConnectionPoint\n");

                    unload();

                    return false;
                }

                result =
                    _controller->queryInterface(
                        Steinberg::Vst::IConnectionPoint::iid,
                        reinterpret_cast<void**>(
                            &controllerConnection));

                if (result !=
                    Steinberg::kResultOk ||
                    !controllerConnection)
                {
                    Logger::Log(
                        "[VST] Controller does not provide IConnectionPoint\n");

                    componentConnection->release();

                    unload();

                    return false;
                }

                /*
                 * Store the connection points.
                 *
                 * queryInterface() gives us an owned reference,
                 * which is transferred into the IPtr objects.
                 */
                _componentConnection =
                    Steinberg::IPtr<
                    Steinberg::Vst::IConnectionPoint>(
                        componentConnection);

                _controllerConnection =
                    Steinberg::IPtr<
                    Steinberg::Vst::IConnectionPoint>(
                        controllerConnection);

                /*
                 * Connect component -> controller.
                 */
                result =
                    _componentConnection->connect(
                        _controllerConnection);

                if (result !=
                    Steinberg::kResultOk)
                {
                    Logger::Log(
                        "[VST] Failed to connect component to controller: result=%d\n",
                        result);

                    unload();

                    return false;
                }

                /*
                 * Connect controller -> component.
                 */
                result =
                    _controllerConnection->connect(
                        _componentConnection);

                if (result !=
                    Steinberg::kResultOk)
                {
                    Logger::Log(
                        "[VST] Failed to connect controller to component: result=%d\n",
                        result);

                    unload();

                    return false;
                }
            }

            /*
             * --------------------------------------------------------
             * Configure processing
             * --------------------------------------------------------
             */

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

            result =
                _processor->setupProcessing(
                    setup);

            if (result !=
                Steinberg::kResultOk)
            {
                Logger::Log(
                    "[VST] setupProcessing failed: result=%d\n",
                    result);

                unload();

                return false;
            }

            /*
             * The component must also be activated before processing.
             *
             * We do this here because VSTAudio will eventually call
             * process() on the processor.
             */
            result =
                _component->setActive(
                    true);

            if (result !=
                Steinberg::kResultOk)
            {
                Logger::Log(
                    "[VST] Failed to activate component: result=%d\n",
                    result);

                unload();

                return false;
            }

            _path = path;

            Logger::Log(
                "[VST] Plugin initialized successfully: %s\n",
                classInfo.name().data());

            return true;
        }

        Logger::Log(
            "[VST] No VST3 audio effect found in module\n");

        unload();

        return false;
    }


    void VSTPlugin::unload()
    {
        _editorName = "VST Editor";

        destroyEditor();

        /*
         * --------------------------------------------------------
         * Disconnect component and controller
         * --------------------------------------------------------
         *
         * The connection points must be disconnected before either
         * the component or controller is terminated/released.
         */
        if (_componentConnection &&
            _controllerConnection)
        {
            Logger::Log(
                "[VST] Disconnecting controller/component\n");

            auto componentResult =
                _componentConnection->disconnect(
                    _controllerConnection);

            Logger::Log(
                "[VST] Component disconnect result=%d\n",
                componentResult);

            auto controllerResult =
                _controllerConnection->disconnect(
                    _componentConnection);

            Logger::Log(
                "[VST] Controller disconnect result=%d\n",
                controllerResult);
        }

        /*
         * Release the connection-point interfaces BEFORE releasing
         * the component and controller they belong to.
         */
        _componentConnection =
            nullptr;

        _controllerConnection =
            nullptr;

        /*
         * Deactivate the component before terminating it.
         */
        if (_component)
        {
            Logger::Log(
                "[VST] Deactivating component\n");

            _component->setActive(
                false);
        }

        /*
         * Terminate the controller and component.
         *
         * IPluginBase::terminate() must be called once for each
         * independently initialized object.
         *
         * If the component also implements the controller,
         * it must only be terminated once.
         */
        if (_component)
        {
            Logger::Log(
                "[VST] Terminating component\n");

            auto* componentPluginBase =
                static_cast<
                Steinberg::IPluginBase*>(
                    _component.get());

            if (componentPluginBase)
            {
                componentPluginBase->terminate();
            }
        }

        if (_controller && !_singleComponent)
        {
            Logger::Log(
                "[VST] Terminating controller\n");

            auto* controllerPluginBase =
                static_cast<
                Steinberg::IPluginBase*>(
                    _controller.get());

            if (controllerPluginBase)
            {
                controllerPluginBase->terminate();
            }
        }

        /*
         * --------------------------------------------------------
         * Release plugin interfaces
         * --------------------------------------------------------
         */
        _processor =
            nullptr;

        _controller =
            nullptr;

        _component =
            nullptr;

        _singleComponent =
            false;

        /*
         * --------------------------------------------------------
         * Release host interfaces
         * --------------------------------------------------------
         */
        if (_componentHandler)
        {
            Logger::Log(
                "[VST] Destroying ComponentHandler\n");

            _componentHandler->release();

            _componentHandler =
                nullptr;
        }

        if (_hostApplication)
        {
            Logger::Log(
                "[VST] Destroying HostApplication\n");

            _hostApplication->release();

            _hostApplication =
                nullptr;
        }

        /*
         * --------------------------------------------------------
         * Finally unload the module.
         * --------------------------------------------------------
         */
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
            _component != nullptr;
    }


    const std::string& VSTPlugin::path() const
    {
        return _path;
    }


    Steinberg::Vst::IComponent*
        VSTPlugin::component() const
    {
        return _component;
    }


    Steinberg::Vst::IEditController*
        VSTPlugin::controller() const
    {
        return _controller;
    }


    Steinberg::Vst::IAudioProcessor*
        VSTPlugin::processor() const
    {
        return _processor;
    }


    bool VSTPlugin::createEditor(
        HINSTANCE instance)
    {
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

        if (!_controller)
        {
            Logger::Log(
                "[VST] Cannot create editor: controller is null\n");

            return false;
        }

        /*
         * Ask the controller for its editor.
         */
        Steinberg::IPlugView* view =
            _controller->createView(
                Steinberg::Vst::ViewType::kEditor);

        if (!view)
        {
            Logger::Log(
                "[VST] Plugin did not provide an editor\n");

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
                _editorName.c_str(),
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