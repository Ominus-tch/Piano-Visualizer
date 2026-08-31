#define _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
#define _SILENCE_ALL_MS_EXT_DEPRECATION_WARNINGS
#include <d3d11.h>
#include <iostream>
#include <Windows.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_dx11.h>
#include <imgui/imgui_internal.h>

#include <vector>
#include <string>
#include <thread>

#include <chrono>
#include <cstdint>
#include <array>
#include <utility>
#include <cstdio>

#include "../util/Logger.h"
#include "../util/camera/Camera.h"
#include "../util/capture/WindowCapture.h"
#include "../util/renderer/VirtualWindowRenderer.h"

#include "../util/DragDrop/FileDropTarget.h"

#include "helpers.h"
#include "gui.h"
#include "config.h"

// MidiVisualizer
#include "MidiVisualizer/helpers/ProgramUtilities.h"
#include "MidiVisualizer/helpers/Configuration.h"
#include "MidiVisualizer/helpers/ResourcesManager.h"
#include "MidiVisualizer/helpers/ImGuiStyle.h"
#include "MidiVisualizer/helpers/System.h"
#include "MidiVisualizer/rendering/scene/MIDISceneLive.h"
#include "MidiVisualizer/rendering/Viewer.h"

// Audio
#include "Audio/AudioEngine.h"

#include "Audio/AudioOutput.h"
#include "Audio/vst/VSTPlugin.h"

void DebugSetup();
void DebugEnd();
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void SetFullscreen(HWND window, bool fullscreen);
bool IsWindowFullscreen(HWND hwnd);
static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
static void RefreshCaptureWindows();

ID3D11Device* gDevice = nullptr;
ID3D11DeviceContext* gContext = nullptr;
IDXGISwapChain* gSwapChain = nullptr;
ID3D11RenderTargetView* gRTV = nullptr;

Viewer* gViewer = nullptr;
audio::AudioEngine* gAudioEngine = nullptr;

HWND gVstEditorWindow = nullptr;

std::atomic<bool> g_shouldExit = false;
BOOL WINAPI ConsoleHandler(
    DWORD signal)
{
    switch (signal)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        g_shouldExit = true;
        return TRUE;
    }

    return FALSE;
}

static bool CreateMidiVisualizer(ID3D11Device* device, ID3D11DeviceContext* context)
{
    // =========================================================
    // MidiVisualizer setup
    // =========================================================

    // Setup resources.
    ResourcesManager::loadResources(device);
    // Create the viewer (passing options to display them)

    D3D11Interface d3dInterface = { device, context };
    gViewer = new Viewer(d3dInterface, 1920, 1080);

    // Ensure we are using the C locale.
    System::forceLocale();

    Config::GetConfigDirectory();

    // This has to be called after glfwInit for the working dir to be OK on macOS.
    const std::vector<std::string> argv = {};
    Configuration config(argv);

    if (config.showHelp) {
        Configuration::printHelp();
        return false;
    }
    if (config.showVersion) {
        Configuration::printVersion();
        return false;
    }

    // Load midi file if specified.
    if (!config.lastMidiPath.empty()) {
        gViewer->loadFile(config.lastMidiPath);
    }
    // Apply custom state.
    State state;
    if (!config.lastConfigPath.empty()) {
        state.load();
    }
    // Apply any extra display argument on top of the existing config.
    state.load(config.args());
    gViewer->setState(state);

    // Connect to MIDI device if specified. We do it after setting the state because there are constraints on the scroll direction when recording.
    // But we don't want to force reverse-scroll when playing back a recorded liveplay.
    if (!config.lastMidiDevice.empty()) {
        gViewer->connectDevice(config.lastMidiDevice);
    }


    // End MidiVisualizer Setup
    return true;
}

LRESULT CALLBACK vstEditorWindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    return DefWindowProc(
        hwnd,
        msg,
        wParam,
        lParam);
}

INT APIENTRY WinMain(HINSTANCE instance, HINSTANCE, PSTR, INT cmd_show)
{

	if (gui::debug)
		DebugSetup();

	if (!Logger::Init())
	{
		AllocConsole();
		freopen_s(&gui::file, "CONOUT$", "w", stdout);

		std::cout << "Unable to initialze logger!\n";
		system("pause");
		fclose(gui::file);
		FreeConsole();
		return 1;
	}

	Logger::Log("Logger Initiliazed!\n");


    HRESULT oleResult = OleInitialize(nullptr);

    if (FAILED(oleResult))
    {
        Logger::Log("Failed to initialize OLE!\n");
        return 1;
    }

	WNDCLASSEX wc{};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = instance;
	wc.lpszClassName = "Overlay class";

	if (!RegisterClassEx(&wc))
	{
		Logger::Log("Failed to register window class!\n");
		return 1;
	}

	const HWND window = CreateWindowEx(
		0,
		wc.lpszClassName,
		"Piano Visualizer",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		1280,
		720,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	if (!window)
	{
		Logger::Log("Failed to create window!\n");
		return 1;
	}

    //// Make VST Plugin Editor
    //WNDCLASSEX vstEditorClass{};
    //vstEditorClass.cbSize = sizeof(WNDCLASSEX);
    //vstEditorClass.lpfnWndProc = vstEditorWindowProc;
    //vstEditorClass.hInstance = instance;
    //vstEditorClass.lpszClassName = "VST Editor Host";

    //if (!RegisterClassEx(&vstEditorClass))
    //{
    //    Logger::Log(
    //        "Failed to register VST editor window class!\n");

    //    Logger::Remove();
    //    return 1;
    //}

    // VST STUFF

    gAudioEngine = new audio::AudioEngine();

    if (!gAudioEngine->initialize())
    {
        Logger::Log("Failed to initialize Audio Engine\n");

        if (gAudioEngine)
        {
            gAudioEngine->shutdown();
            delete gAudioEngine;
            gAudioEngine = nullptr;
        }

        Logger::Remove();
        return 1;
    }

    std::filesystem::path previousVstPath = "";
    if (Config::LoadVSTConfig(previousVstPath))
    {
        if (!previousVstPath.empty())
        {
            if (gAudioEngine->loadPlugin(previousVstPath.string()))
            {
                if (gAudioEngine->start())
                {

                    if (gAudioEngine->plugin()->createEditor(instance))
                    {
                        Logger::Log(
                            "Editor created!\n"
                        );
                    }
                    else
                    {
                        Logger::Log(
                            "Failed to create editor!\n"
                        );
                    }
                }
                else
                {
                    Logger::Log(
                        "Failed to start Audio Engine!\n"
                    );
                }
            }
            else
            {
                Logger::Log(
                    "Failed to load plugin %s\n",
                    previousVstPath.string().c_str()
                );
            }
        }
    }

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = window;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	ID3D11Device*			  device				{ nullptr };
	ID3D11DeviceContext*	  device_context		{ nullptr };
	IDXGISwapChain*			  swap_chain			{ nullptr };
	ID3D11RenderTargetView*	  render_target_view	{ nullptr };
	D3D_FEATURE_LEVEL		  level					{		  };

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        2U,
        D3D11_SDK_VERSION,
        &sd,
        &swap_chain,
        &device,
        &level,
        &device_context
    );

	if (FAILED(hr))
	{
		Logger::Log("Failed to create D3D11 device and swap chain!\n");

		DestroyWindow(window);
		UnregisterClass(wc.lpszClassName, wc.hInstance);

		Logger::Remove();

		return 1;
	}



	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer)
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	}
	else
	{
		Logger::Log("Back buffer invalid! Exiting...\n");
		Logger::Remove();
		return 1;
	}

	gDevice = device;
	gContext = device_context;
	gSwapChain = swap_chain;
	gRTV = render_target_view;

    if (!gRTV)
    {
        Logger::Log("Render Target View invalid!\n");
        Logger::Remove();
        return 1;
    }

	ShowWindow(window, cmd_show);
	UpdateWindow(window);

	Logger::Log("Window created!\n");

	Logger::Log("Setting up ImGui...\n\n");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;

    ImFontConfig font;
    ImGui::configureFont(font);
    ImGui::configureStyle();

    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(device, device_context);

	static Camera camera;
	if (!camera.Initialize(
		gDevice,
		gContext, 0)) {
		Logger::Log("Failed to initialize camera!\n");

        Logger::Remove();
		return 1;
	}

    static WindowCapture windowCapture;

    if (!windowCapture.Initialize(
        gDevice,
        gContext
    ))
    {
        Logger::Log(
            "Failed to initialize window capture!\n"
        );

        Logger::Remove();
        return 1;
    }

    static VirtualWindowRenderer virtualWindowRenderer;

    static bool drawVisualizer = false;

    if (!CreateMidiVisualizer(gDevice, gContext))
    {
        Logger::Log("Failed to create midi visualizer!\n");

        Logger::Remove();
        return 1;
    }
	
    FileDropTarget vstDropTarget({
    L".vst3"
        });

    if (!vstDropTarget.Register(window))
    {
        Logger::Log(
            "Failed to register VST drag-drop target!\n");
    }
	
	auto startTime = std::chrono::steady_clock::now();
	float elapsedTime = 0;

	// Declare string variables to hold the button texts (Weird ImGui bug with refreshing the window if its not static)
	
	auto prevTime = std::chrono::steady_clock::now();

	Logger::Log("Loop starting...\n");
	while (true)
	{
		auto currentTime = std::chrono::steady_clock::now();
		prevTime = currentTime;

		elapsedTime = static_cast<float>(std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count());

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
				gui::running = false;
		}

		if (!gui::running || g_shouldExit)
		{
			Logger::Log("Loop Ending...\n");
			break;
		}

        if (ImGui::IsKeyPressed(ImGuiKey_I))
        {
            gui::showSettings = !gui::showSettings;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F))
        {
            gui::fullscreen = !gui::fullscreen;
            SetFullscreen(window, gui::fullscreen);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        camera.Update();

        if (gViewer)
        {
            gViewer->setShowGUI(gui::showSettings);
            auto liveScene = std::dynamic_pointer_cast<MIDISceneLive>(gViewer->scene());
            if (liveScene)
                liveScene->setAudioEngine(gAudioEngine);
        }

        if (gui::renderer ==
            gui::VisualizerRenderer::OtherVisualizer)
        {
            HWND desiredWindow =
                gui::selectedCaptureWindow;

            if (desiredWindow != gui::activeCaptureWindow)
            {
                // Stop previous capture
                if (gui::windowCaptureRunning)
                {
                    windowCapture.Stop();
                    gui::windowCaptureRunning = false;
                    gui::activeCaptureWindow = nullptr;
                }

                // Start new capture
                if (desiredWindow && IsWindow(desiredWindow))
                {
                    if (!gui::windowCaptureRunning)
                    {

                        if (!windowCapture.Start(desiredWindow))
                        {
                            Logger::Log(
                                "Failed to start window capture!\n"
                            );

                            windowCapture.Stop();
                        }
                        else
                        {
                            gui::windowCaptureRunning = true;
                            gui::activeCaptureWindow =desiredWindow;

                            Logger::Log(
                                "Window capture started.\n"
                            );
                        }
                    }
                }
            }
        }
        else
        {
            // Built-in renderer selected.
            // Make absolutely sure external capture is stopped.

            if (gui::windowCaptureRunning)
            {
                windowCapture.Stop();

                gui::windowCaptureRunning = false;
                gui::activeCaptureWindow = nullptr;

                Logger::Log(
                    "Window capture stopped.\n"
                );
            }
        }


        static bool choosingPolygonPoints = false;

        static int polygonClickCount = 0;

        // The four polygon points, stored in camera coordinates.
        //
        // IMPORTANT ORDER:
        //
        // 0 = Top-left
        // 1 = Bottom-left
        // 2 = Bottom-right
        // 3 = Top-right
        //
        static std::vector<ImVec2> savedPolygonPoints;
        static std::vector<ImVec2> polygonPoints;

        // -----------------------------------------------------
        // How far the virtual piano extends away from the
        // bottom edge.
        //
        // 1.0 = exactly reaches the selected top edge.
        //
        // Start with 1.0.
        // -----------------------------------------------------

        static float heightScale = 1.0f;
        static float virtualWidth = 1920.0f;
        static float virtualDepth = 1080.0f;

        static float planeWidth = 16.f / 9.f;
        static float planeDepth = 1.0f;

        static float horizontalFovDegrees = 90.0f;

        static float surfaceXOffset = 0.0f;
        static float surfaceYOffset = 0.0f;
        static float surfaceZOffset = 0.0f;

        static std::string configSaveMessage;

        std::call_once(gui::onceFlag, [&]() {
            if (!Config::LoadPianoConfig(
                polygonPoints,
                horizontalFovDegrees,
                planeWidth,
                planeDepth,
                surfaceXOffset,
                surfaceYOffset,
                surfaceZOffset
            ))
            {
                Logger::Log("No piano config found. Using defaults.\n");
            }
		});

        constexpr float color[4]{ 0.f, 0.f, 0.f, 1.f };
        gContext->OMSetRenderTargets(1U, &gRTV, nullptr);
        gContext->ClearRenderTargetView(gRTV, color);

        if (drawVisualizer)
        {
            SystemAction action =
                gViewer->draw(
                    DEBUG_SPEED * float(System::time())
                );

            gContext->OMSetRenderTargets(
                1,
                &gRTV,
                nullptr
            );

            glm::ivec2 wSize = gViewer->getSize();

            D3D11_VIEWPORT viewport{};
            viewport.TopLeftX = 0.0f;
            viewport.TopLeftY = 0.0f;
            viewport.Width = static_cast<float>(wSize.x);
            viewport.Height = static_cast<float>(wSize.y);
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;

            gContext->RSSetViewports(1, &viewport);

            drawVisualizer = false;
        }
        
        // =========================================================
        // CAMERA
        // =========================================================

        if (camera.IsOpen())
        {
            ImGuiViewport* viewport =
                ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(
                viewport->WorkPos,
                ImGuiCond_Always
            );

            ImGui::SetNextWindowSize(
                viewport->WorkSize,
                ImGuiCond_Always
            );

            ImGuiWindowFlags flags =
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoMove;

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowPadding,
                ImVec2(0, 0)
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowBorderSize,
                0.0f
            );

            ImGui::PushStyleVar(
                ImGuiStyleVar_WindowRounding,
                0.0f
            );

            ImGui::Begin(
                "Camera Feed",
                nullptr,
                flags
            );

            ImGuiWindow* camWindow = ImGui::GetCurrentWindow();
            if (choosingPolygonPoints)
                ImGui::BringWindowToDisplayFront(camWindow);
            else
                ImGui::BringWindowToDisplayBack(camWindow);

            
            // ToolTip window
            if (choosingPolygonPoints)
            {
                ImGuiIO& io = ImGui::GetIO();

                ImGui::SetNextWindowPos(
                    ImVec2(io.MousePos.x + 12.f, io.MousePos.y + 12.f),
                    ImGuiCond_Always
                );

                ImGui::SetNextWindowBgAlpha(0.9f);

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));

                ImGuiWindowFlags ttFlags =
                    ImGuiWindowFlags_NoDecoration |
                    ImGuiWindowFlags_AlwaysAutoResize |
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoFocusOnAppearing |
                    ImGuiWindowFlags_NoNav;

                ImGui::Begin("MouseTooltip", nullptr, ttFlags);

                ImGuiWindow* ttWindow = ImGui::GetCurrentWindow();
                ImGui::BringWindowToDisplayFront(ttWindow);

                switch (polygonClickCount)
                {
                case 0:
                    ImGui::Text("P1. Click    TOP-LEFT    corner of the piano (ESC to cancel)");
                    break;
                case 1:
                    ImGui::Text("P2: Click   BOTTOM-LEFT  corner of the piano (ESC to cancel)");
                    break;
                case 2:
                    ImGui::Text("P3: Click  BOTTOM-RIGHT  corner of the piano (ESC to cancel)");
                    break;
                case 3:
                    ImGui::Text("P4: Click    TOP-RIGHT   corner of the piano (ESC to cancel)");
                    break;
                }

                ImGui::End();

                ImGui::PopStyleVar(2);

                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                {
                    polygonPoints = savedPolygonPoints;
                    choosingPolygonPoints = false;
                }
            }


            // =====================================================
            // IMAGE
            // =====================================================

            ImVec2 imagePos =
                ImGui::GetCursorScreenPos();

            ImVec2 available =
                ImGui::GetContentRegionAvail();

            float cameraWidth =
                static_cast<float>(
                    camera.GetWidth()
                    );

            float cameraHeight =
                static_cast<float>(
                    camera.GetHeight()
                    );


            if (cameraWidth > 0.0f &&
                cameraHeight > 0.0f)
            {
                float aspect =
                    cameraWidth / cameraHeight;

                float width =
                    available.x;

                float height =
                    width / aspect;


                // Keep aspect ratio
                if (height > available.y)
                {
                    height =
                        available.y;

                    width =
                        height * aspect;
                }


                // =================================================
                // Draw camera
                // =================================================

                ImGui::Image(
                    (ImTextureID)camera.GetTexture(),
                    ImVec2(
                        width,
                        height
                    )
                );

                auto CameraToScreen =
                    [&](float cameraX, float cameraY)
                    {
                        float screenX =
                            imagePos.x +
                            cameraX * (width / cameraWidth);

                        float screenY =
                            imagePos.y +
                            cameraY * (height / cameraHeight);

                        return ImVec2(
                            screenX,
                            screenY
                        );
                    };


                // =================================================
                // DRAW OVERLAY
                // =================================================

                ImDrawList* drawList =
                    ImGui::GetWindowDrawList();

                {
                    if (polygonPoints.size() == 4)
                    {
                        ImVec2 P1 =
                            polygonPoints[0];

                        ImVec2 P2 =
                            polygonPoints[1];

                        ImVec2 P3 =
                            polygonPoints[2];

                        ImVec2 P4 =
                            polygonPoints[3];


                        PianoCameraPose pose =
                            CalculatePianoCameraPose(
                                P1,
                                P2,
                                P3,
                                P4,
                                cameraWidth,
                                cameraHeight,
                                horizontalFovDegrees
                            );


                        if (pose.valid)
                        {
                            float worldHeight =
                                -planeDepth *
                                heightScale;

                            float x1, y1;
                            float x2, y2;
                            float x3, y3;
                            float x4, y4;

                            bool valid1 =
                                ProjectPianoPoint(
                                    pose,
                                    surfaceXOffset,
                                    surfaceYOffset,
                                    surfaceZOffset,
                                    x1,
                                    y1
                                );

                            bool valid2 =
                                ProjectPianoPoint(
                                    pose,
                                    planeWidth + surfaceXOffset,
                                    surfaceYOffset,
                                    surfaceZOffset,
                                    x2,
                                    y2
                                );

                            bool valid3 =
                                ProjectPianoPoint(
                                    pose,
                                    planeWidth+surfaceXOffset,
                                    surfaceYOffset,
                                    worldHeight + surfaceZOffset,
                                    x3,
                                    y3
                                );

                            bool valid4 =
                                ProjectPianoPoint(
                                    pose,
                                    surfaceXOffset,
                                    surfaceYOffset,
                                    worldHeight + surfaceZOffset,
                                    x4,
                                    y4
                                );

                            if (
                                valid1 &&
                                valid2 &&
                                valid3 &&
                                valid4
                                )
                            {
                                ImVec2 bottomLeft =
                                    CameraToScreen(
                                        x1,
                                        y1
                                    );

                                ImVec2 bottomRight =
                                    CameraToScreen(
                                        x2,
                                        y2
                                    );

                                ImVec2 topRight =
                                    CameraToScreen(
                                        x3,
                                        y3
                                    );

                                ImVec2 topLeft =
                                    CameraToScreen(
                                        x4,
                                        y4
                                    );


                                // =================================================
                                // DRAW QUAD
                                // =================================================

                                ImVec2 virtualWindow[4] =
                                {
                                    topLeft,
                                    topRight,
                                    bottomRight,
                                    bottomLeft
                                };

                                ID3D11ShaderResourceView* texture = nullptr;
                                if (gui::renderer == gui::VisualizerRenderer::OtherVisualizer) {
                                    windowCapture.Update();
                                    texture = windowCapture.GetTexture();
                                }
                                else if (gui::renderer == gui::VisualizerRenderer::BuiltInVisualizer)
                                {
                                    drawVisualizer = true;
                                    texture = gViewer->getTexture();
                                }

                                if (texture)
                                {
                                    ImTextureID textureId =
                                        reinterpret_cast<ImTextureID>(
                                            texture
                                            );


                                    VirtualWindowRenderer::Settings settings;

                                    settings.gridX = 64;
                                    settings.gridY = 36;
                                    settings.drawDebugLines = gui::showDebugLines;


                                    virtualWindowRenderer.Render(
                                        drawList,
                                        textureId,

                                        topLeft,
                                        topRight,
                                        bottomRight,
                                        bottomLeft,

                                        settings
                                    );
                                }

                                if (gui::showDebugLines) {
                                    auto pts = std::vector<ImVec2>();
                                    pts.push_back(topLeft);
                                    pts.push_back(topRight);
                                    pts.push_back(bottomRight);
                                    pts.push_back(bottomLeft);

                                    for (size_t i = 0;
                                        i < polygonPoints.size();
                                        ++i)
                                    {
                                        drawList->AddCircleFilled(
                                            pts[i],
                                            7.0f,
                                            IM_COL32(
                                                255,
                                                255,
                                                255,
                                                255
                                            )
                                        );


                                        drawList->AddCircle(
                                            pts[i],
                                            8.0f,
                                            IM_COL32(
                                                0,
                                                0,
                                                0,
                                                255
                                            ),
                                            16,
                                            1.5f
                                        );
                                    }


                                    // =================================================
                                    // OUTLINE
                                    // =================================================

                                    for (int i = 0; i < 4; ++i)
                                    {
                                        int next =
                                            (i + 1) % 4;

                                        drawList->AddLine(
                                            virtualWindow[i],
                                            virtualWindow[next],
                                            IM_COL32(
                                                0,
                                                150,
                                                255,
                                                230
                                            ),
                                            2.0f
                                        );
                                    }
                                }
                            }
                        }
                    }
                }

                if (gui::showDebugLines) {

                    // =================================================
                    // DRAW SELECTED PIANO QUADRILATERAL
                    // =================================================

                    if (polygonPoints.size() >= 2)
                    {
                        ImVec2 points[4];

                        for (size_t i = 0;
                            i < polygonPoints.size() &&
                            i < 4;
                            ++i)
                        {
                            points[i] =
                                CameraToScreen(
                                    polygonPoints[i].x,
                                    polygonPoints[i].y
                                );
                        }


                        // -------------------------------------------------
                        // Filled polygon
                        // -------------------------------------------------

                        if (polygonPoints.size() >= 3)
                        {
                            drawList->AddConvexPolyFilled(
                                points,
                                static_cast<int>(
                                    polygonPoints.size()
                                    ),
                                IM_COL32(
                                    0,
                                    255,
                                    0,
                                    80
                                )
                            );
                        }


                        // -------------------------------------------------
                        // Polygon outline
                        // -------------------------------------------------

                        for (size_t i = 0;
                            i < polygonPoints.size();
                            ++i)
                        {
                            size_t next =
                                (i + 1) %
                                polygonPoints.size();

                            drawList->AddLine(
                                points[i],
                                points[next],
                                IM_COL32(
                                    0,
                                    255,
                                    0,
                                    255
                                ),
                                2.0f
                            );
                        }


                        // -------------------------------------------------
                        // Polygon points
                        // -------------------------------------------------

                        for (size_t i = 0;
                            i < polygonPoints.size();
                            ++i)
                        {
                            drawList->AddCircleFilled(
                                points[i],
                                7.0f,
                                IM_COL32(
                                    255,
                                    255,
                                    255,
                                    255
                                )
                            );


                            drawList->AddCircle(
                                points[i],
                                8.0f,
                                IM_COL32(
                                    0,
                                    0,
                                    0,
                                    255
                                ),
                                16,
                                1.5f
                            );


                            // -------------------------------------------------
                            // Point label
                            // -------------------------------------------------

                            const char* label = "";

                            switch (i)
                            {
                            case 0:
                                label = "P1 - Top Left";
                                break;

                            case 1:
                                label = "P2 - Bottom Left";
                                break;

                            case 2:
                                label = "P3 - Bottom Right";
                                break;

                            case 3:
                                label = "P4 - Top Right";
                                break;
                            }

                            drawList->AddText(
                                ImVec2(
                                    points[i].x + 10.0f,
                                    points[i].y - 10.0f
                                ),
                                IM_COL32(
                                    255,
                                    255,
                                    255,
                                    255
                                ),
                                label
                            );
                        }
                    }
                }


                // =================================================
                // CLICK DETECTION
                // =================================================

                if (choosingPolygonPoints &&
                    ImGui::IsItemHovered() &&
                    ImGui::IsMouseClicked(
                        ImGuiMouseButton_Left
                    ))
                {



                    ImVec2 mousePos =
                        ImGui::GetMousePos();


                    // -------------------------------------------------
                    // Screen -> displayed image coordinates
                    // -------------------------------------------------

                    float imageX =
                        mousePos.x -
                        imagePos.x;

                    float imageY =
                        mousePos.y -
                        imagePos.y;


                    // -------------------------------------------------
                    // Displayed image -> camera coordinates
                    // -------------------------------------------------

                    float scaleX =
                        cameraWidth /
                        width;

                    float scaleY =
                        cameraHeight /
                        height;


                    float cameraX =
                        imageX *
                        scaleX;

                    float cameraY =
                        imageY *
                        scaleY;


                    // -------------------------------------------------
                    // Clamp
                    // -------------------------------------------------

                    cameraX =
                        std::clamp(
                            cameraX,
                            0.0f,
                            cameraWidth - 1.0f
                        );

                    cameraY =
                        std::clamp(
                            cameraY,
                            0.0f,
                            cameraHeight - 1.0f
                        );


                    // =================================================
                    // ADD POINT
                    // =================================================

                    if (polygonPoints.size() < 4)
                    {
                        polygonPoints.push_back(
                            ImVec2(
                                cameraX,
                                cameraY
                            )
                        );

                        polygonClickCount++;


                        const char* pointName = "";

                        switch (polygonClickCount)
                        {
                        case 1:
                            pointName = "Top-left";
                            break;

                        case 2:
                            pointName = "Bottom-left";
                            break;

                        case 3:
                            pointName = "Bottom-right";
                            break;

                        case 4:
                            pointName = "Top-right";
                            break;
                        }


                        Logger::Log(
                            "P" +
                            std::to_string(
                                polygonClickCount
                            ) +
                            " (" +
                            std::string(pointName) +
                            "): (" +
                            std::to_string(
                                static_cast<int>(cameraX)
                            ) +
                            ", " +
                            std::to_string(
                                static_cast<int>(cameraY)
                            ) +
                            ")\n"
                        );


                        // -------------------------------------------------
                        // Finished
                        // -------------------------------------------------

                        if (polygonPoints.size() >= 4)
                        {
                            choosingPolygonPoints = false;

                            Logger::Log(
                                "Piano corner selection complete.\n"
                            );
                        }
                    }
                }
            }


            ImGui::End();

            ImGui::PopStyleVar(3);
        }

        // =========================================================
        // SETTINGS WINDOW
        // =========================================================

    

        if (gui::showSettings) {

            if (ImGui::Begin("Settings (Press I to hide)", &gui::showSettings, ImGuiWindowFlags_AlwaysAutoResize)) {


                // ---------------------------------------------------------
                // Choose 4 polygon points
                // ---------------------------------------------------------

                if (ImGui::Checkbox("Fullscreen", &gui::fullscreen))
                {
                    SetFullscreen(window, gui::fullscreen);
                }

                if (!choosingPolygonPoints)
                {
                    if (ImGui::Button("Choose Points"))
                    {
                        savedPolygonPoints = polygonPoints;
                        polygonPoints.clear();

                        polygonClickCount = 0;

                        choosingPolygonPoints = true;

                        Logger::Log(
                            "Waiting for 4 piano corner points...\n"
                        );
                    }
                }

                ImGui::Separator();

                ImGui::Text("Visualizer Renderer");

                const char* rendererName = nullptr;

                switch (gui::renderer)
                {
                case gui::VisualizerRenderer::BuiltInVisualizer:
                    rendererName = "Built-in visualizer";
                    break;

                case gui::VisualizerRenderer::OtherVisualizer:
                    rendererName = "Capture other visualizer";
                    break;
                default:
                    rendererName = "Unknown";
                    break;
                }

                if (ImGui::BeginCombo(
                    "Renderer",
                    rendererName
                ))
                {
                    if (ImGui::Selectable(
                        "Built-in visualizer",
                        gui::renderer ==
                        gui::VisualizerRenderer::BuiltInVisualizer
                    ))
                    {
                        gui::renderer =
                            gui::VisualizerRenderer::BuiltInVisualizer;
                    }

                    if (ImGui::Selectable(
                        "Capture other visualizer",
                        gui::renderer ==
                        gui::VisualizerRenderer::OtherVisualizer
                    ))
                    {
                        gui::renderer =
                            gui::VisualizerRenderer::OtherVisualizer;
                    }

                    ImGui::EndCombo();
                }

                if (gui::renderer ==
                    gui::VisualizerRenderer::OtherVisualizer)
                {
                    ImGui::Separator();

                    ImGui::Text("Capture Window");

                    if (ImGui::Button("Refresh Windows"))
                    {
                        RefreshCaptureWindows();
                    }

                    const char* selectedName =
                        "Select a window";

                    int selectedWindowIndex =
                        gui::selectedCaptureWindowIndex;

                    std::string selectedTitle;

                    if (selectedWindowIndex >= 0 &&
                        selectedWindowIndex < static_cast<int>(gui::captureWindows.size()))
                    {
                        const std::wstring& ws = gui::captureWindows[selectedWindowIndex].title;

                        if (!ws.empty())
                        {
                            int size = WideCharToMultiByte(
                                CP_UTF8,
                                0,
                                ws.data(),
                                static_cast<int>(ws.size()),
                                nullptr,
                                0,
                                nullptr,
                                nullptr
                            );

                            selectedTitle.resize(size);

                            WideCharToMultiByte(
                                CP_UTF8,
                                0,
                                ws.data(),
                                static_cast<int>(ws.size()),
                                selectedTitle.data(),
                                size,
                                nullptr,
                                nullptr
                            );
                        }

                        selectedName = selectedTitle.c_str();
                    }

                    if (ImGui::BeginCombo(
                        "Window",
                        selectedName
                    ))
                    {
                        for (
                            int i = 0;
                            i < static_cast<int>(gui::captureWindows.size());
                            ++i
                            )
                        {
                            std::string title(
                                gui::captureWindows[i].title.begin(),
                                gui::captureWindows[i].title.end()
                            );

                            bool selected =
                                selectedWindowIndex == i;

                            if (ImGui::Selectable(
                                title.c_str(),
                                selected
                            ))
                            {
                                gui::selectedCaptureWindowIndex = i;

                                gui::selectedCaptureWindow =
                                    gui::captureWindows[i].hwnd;
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }

                        ImGui::EndCombo();
                    }
                }

                ImGui::Separator();

                ImGui::Checkbox(
                    "Show Debug Lines",
                    &gui::showDebugLines
				);

                ImGui::Separator();
                ImGui::SliderFloat(
                    "Horizontal FOV Degrees",
                    &horizontalFovDegrees,
                    0.1f,
                    90.0f,
                    "%.3f"
                );

                ImGui::SliderFloat(
                    "Plane Width Scale",
                    &planeWidth,
                    0.1f,
                    2.0f,
                    "%.3f"
                );

                ImGui::SliderFloat(
                    "Plane Depth Scale",
                    &planeDepth,
                    0.1f,
                    5.0f,
                    "%.3f"
                );

                ImGui::SliderFloat(
                    "X Offset",
                    &surfaceXOffset,
                    -1.f,
                    1.f,
                    "%.3f"
                );

                ImGui::SliderFloat(
                    "Y Offset",
                    &surfaceYOffset,
                    -1.f,
                    1.f,
                    "%.3f"
                );
                
                ImGui::SliderFloat(
                    "Z Offset",
                    &surfaceZOffset,
                    -1.f,
                    1.f,
                    "%.3f"
                );

                ImGui::Separator();

                if (ImGui::Button("Save Piano Configuration"))
                {
                    if (Config::SavePianoConfig(
                        polygonPoints,
                        horizontalFovDegrees,
                        planeWidth,
                        planeDepth,
                        surfaceXOffset,
                        surfaceYOffset,
                        surfaceZOffset
                    ))
                    {
                        // Optional status message
                        configSaveMessage = "Configuration saved!";
                    }
                    else
                    {
                        configSaveMessage = "Failed to save configuration!";
                    }
                }

                ImGui::SameLine();

                if (ImGui::Button("Load Piano Configuration"))
                {
                    if (Config::LoadPianoConfig(
                        polygonPoints,
                        horizontalFovDegrees,
                        planeWidth,
                        planeDepth,
                        surfaceXOffset,
                        surfaceYOffset,
                        surfaceZOffset
                    ))
                    {
                        // Important because we now have four points.
                        polygonClickCount = 4;

                        configSaveMessage = "Configuration loaded!";
                    }
                    else
                    {
                        configSaveMessage = "No valid configuration found!";
                    }
                }

                if (!configSaveMessage.empty())
                {
                    ImGui::Text(
                        "%s",
                        configSaveMessage.c_str()
                    );
                }


            }
            ImGui::End();
        }

        {
            if (vstDropTarget.isDragging())
            {
                ImGuiViewport* viewport = ImGui::GetMainViewport();

                ImDrawList* drawList =
                    ImGui::GetForegroundDrawList();

                drawList->AddRectFilled(
                    viewport->WorkPos,
                    ImVec2(
                        viewport->WorkPos.x + viewport->WorkSize.x,
                        viewport->WorkPos.y + viewport->WorkSize.y
                    ),
                    IM_COL32(255, 255, 255, 20)
                );
            }

            auto droppedFiles = vstDropTarget.consumeDroppedFiles();

            if (!droppedFiles.empty())
            {
                const std::filesystem::path& vstPath = droppedFiles[0];

                if (vstPath != gAudioEngine->vstPath())
                {

                    gAudioEngine->stop();
                    gAudioEngine->unLoadPlugin();

                    if (gAudioEngine->loadPlugin(vstPath.string()))
                    {
                        Config::SaveVSTConfig(vstPath);

                        if (gAudioEngine->start())
                        {
                            Logger::Log(
                                "Audio Engine started!\n"
                            );

                            gAudioEngine->plugin()->createEditor(instance);
                        }
                    }
                }
            }
        }


        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        gSwapChain->Present(1U, 0U);
	}


	Logger::Log("\n");
	Logger::Log("Exit Procedure...\n");
	Logger::Log("Shutting down ImGui...\n");

    // Audio
    gAudioEngine->stop();
    gAudioEngine->shutdown();

	camera.Shutdown();
    vstDropTarget.Unregister();

    OleUninitialize();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	if (gSwapChain)
		gSwapChain->Release();

	if (gContext)
		gContext->Release();

	if (gDevice)
		gDevice->Release();

	if (gRTV)
		gRTV->Release();


	Logger::Log("Destroying Window...\n");

	DestroyWindow(window);
	UnregisterClass(wc.lpszClassName, wc.hInstance);

	Logger::Log("Exiting...\n");
	Logger::Remove();

	if (gui::debug)
		DebugEnd();

	return 0;
} // end WinMain

void DebugSetup()
{
    gui::debug = true;

    AllocConsole();

    freopen_s(
        &gui::file,
        "CONOUT$",
        "w",
        stdout);

    SetConsoleCtrlHandler(
        ConsoleHandler,
        TRUE);

    std::cout <<
        "Debug Mode Started!\n";
}


void DebugEnd()
{
    SetConsoleCtrlHandler(
        ConsoleHandler,
        FALSE);

    gui::debug = false;

    if (gui::file)
    {
        fclose(gui::file);
        gui::file = nullptr;
    }

    FreeConsole();
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void CleanupRenderTarget()
{
	if (gRTV)
	{
		gRTV->Release();
		gRTV = nullptr;
	}
}

void CreateRenderTarget()
{
	ID3D11Texture2D* backBuffer = nullptr;
	gSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));

	if (backBuffer)
	{
		gDevice->CreateRenderTargetView(backBuffer, nullptr, &gRTV);
		backBuffer->Release();
	}
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{

	if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
		return true;

	switch (message)
	{
		case WM_CREATE:
		{
			// Install the low-level hooks
			
			//gui::mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(nullptr), 0);
			break;
		}
		case WM_SIZE:
		{
			if (gDevice && wParam != SIZE_MINIMIZED)
			{
				CleanupRenderTarget();


                HRESULT hr = gSwapChain->ResizeBuffers(
					0,
					(UINT)LOWORD(lParam),
					(UINT)HIWORD(lParam),
					DXGI_FORMAT_UNKNOWN,
					0
				);

				CreateRenderTarget();

				if (gViewer)
				    gViewer->resize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
			}
			return 0;
		}
        case WM_KEYDOWN:
            if (gViewer)
            {
                gViewer->keyPressed(
                    static_cast<int>(wParam),
                    1
                );
            }
            break;
		case WM_DESTROY: 
		{
			// Remove the low-level hooks
			//UnhookWindowsHookEx(gui::mouse_hook);
			PostQuitMessage(EXIT_SUCCESS);
			return EXIT_SUCCESS;
		}
		default:
		{
			break;
		}
	}

	return DefWindowProc(window, message, wParam, lParam);
}

void SetFullscreen(HWND window, bool fullscreen)
{
    static WINDOWPLACEMENT previousPlacement = {
        sizeof(WINDOWPLACEMENT)
    };

    if (fullscreen)
    {
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);

        if (GetWindowPlacement(window, &previousPlacement) &&
            GetMonitorInfo(
                MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                &monitorInfo))
        {
            SetWindowLong(
                window,
                GWL_STYLE,
                WS_POPUP | WS_VISIBLE
            );

            SetWindowPos(
                window,
                HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED
            );
        }
    }
    else
    {
        SetWindowLong(
            window,
            GWL_STYLE,
            WS_OVERLAPPEDWINDOW | WS_VISIBLE
        );

        SetWindowPlacement(window, &previousPlacement);

        SetWindowPos(
            window,
            nullptr,
            0, 0, 0, 0,
            SWP_NOMOVE |
            SWP_NOSIZE |
            SWP_NOZORDER |
            SWP_FRAMECHANGED
        );
    }
}

bool IsWindowFullscreen(HWND hwnd)
{
	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);

	if (GetWindowPlacement(hwnd, &wp))
	{
		return (wp.showCmd == SW_SHOWMAXIMIZED);
	}

	return false;
}

static BOOL CALLBACK EnumWindowsProc(
    HWND hwnd,
    LPARAM lParam
)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    if (hwnd == GetShellWindow())
        return TRUE;

    int length = GetWindowTextLengthW(hwnd);

    if (length <= 0)
        return TRUE;

    std::wstring title(
        length,
        L'\0'
    );

    GetWindowTextW(
        hwnd,
        title.data(),
        length + 1
    );

    if (title.empty())
        return TRUE;

    auto* windows =
        reinterpret_cast<
        std::vector<gui::CaptureWindowEntry>*
        >(lParam);

    windows->push_back({
        hwnd,
        title
        });

    return TRUE;
}

static void RefreshCaptureWindows()
{
    gui::captureWindows.clear();

    EnumWindows(
        EnumWindowsProc,
        reinterpret_cast<LPARAM>(
            &gui::captureWindows
            )
    );
}