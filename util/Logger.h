#pragma once
#include <filesystem>
#include <Windows.h>
#include <iostream>

namespace fs = std::filesystem;

class Logger {
private:
	static inline HANDLE file = nullptr;
public:
	static bool Init();
	static bool Remove();
	static void Log(const char* format, ...);
	static void Log(const std::string& message);
};