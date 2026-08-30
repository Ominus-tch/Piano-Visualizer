#pragma once

#include <imgui/imgui.h>

#include <Windows.h>

#include <chrono>
#include <array>
#include <mutex>

namespace gui
{

	// ImGui
	bool running = true;
	bool fullscreen = false;

	// Program
	int targetFrameRate = 240;

	bool debug = true;
	const char* debugtext = "";
	FILE* file = nullptr;

	bool bIsOpen = true;
	bool opening = true;
	bool closing = false;
	ImVec2 ImWindowNeededSize;
	ImVec2 ImWindowSize;
	ImVec2 ImWindowSizeCached;

	static bool showSettings = true;
	static bool showDebugLines = true;

	bool choosingPoints = false;

	int pointsSelected = 0;

	std::array<std::pair<int, int>, 4> selectedPoints{};

	std::once_flag onceFlag;

	enum class VisualizerRenderer
	{
		BuiltInVisualizer,
		OtherVisualizer
	};

	inline VisualizerRenderer renderer =
		VisualizerRenderer::BuiltInVisualizer;

	inline HWND selectedCaptureWindow = nullptr;
	inline int selectedCaptureWindowIndex = -1;

	struct CaptureWindowEntry
	{
		HWND hwnd;
		std::wstring title;
	};

	static std::vector<CaptureWindowEntry> captureWindows;

	static bool windowCaptureRunning = false;
	static HWND activeCaptureWindow = nullptr;
}