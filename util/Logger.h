#pragma once

#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

class Logger
{
private:
    static inline HANDLE m_file = nullptr;
    static inline bool m_console = true;
    static inline std::string m_offsetStr;

public:
    static bool Init(bool console = true);
    static bool Remove();

    static void Log(const char* format, ...);
    static void Log(const std::string& message);
};