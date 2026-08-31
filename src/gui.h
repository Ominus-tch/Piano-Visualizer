#pragma once

#include <imgui/imgui.h>

#include <Windows.h>
#include <shobjidl.h>
#include <filesystem>
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

	std::filesystem::path OpenFileDialog()
	{
		IFileOpenDialog* dialog = nullptr;

		HRESULT hr = CoCreateInstance(
			CLSID_FileOpenDialog,
			nullptr,
			CLSCTX_ALL,
			IID_PPV_ARGS(&dialog)
		);

		if (FAILED(hr))
			return {};

		hr = dialog->Show(nullptr);

		if (FAILED(hr))
		{
			dialog->Release();
			return {};
		}

		IShellItem* item = nullptr;

		hr = dialog->GetResult(&item);

		if (FAILED(hr))
		{
			dialog->Release();
			return {};
		}

		PWSTR filePath = nullptr;

		hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);

		std::filesystem::path result;

		if (SUCCEEDED(hr))
		{
			result = filePath;
			CoTaskMemFree(filePath);
		}

		item->Release();
		dialog->Release();

		return result;
	}
}

