#include "Logger.h"

bool Logger::Init()
{
	fs::path log;
	wchar_t buf[MAX_PATH];
	if (!GetModuleFileNameW(NULL, buf, MAX_PATH)) return false;
	log = fs::path(buf).remove_filename() / "log.txt";
	file = CreateFileW(log.wstring().c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	return file != INVALID_HANDLE_VALUE;
}

bool Logger::Remove()
{
    Log("Removing Logger...\n");
	if (!file) return true;
	return CloseHandle(file);
}

void Logger::Log(const char* format, ...)
{

    SYSTEMTIME rawtime;
    GetSystemTime(&rawtime);

    char buf[MAX_PATH];
    auto size = GetTimeFormatA(LOCALE_CUSTOM_DEFAULT, 0, &rawtime, "[HH':'mm':'ss] ", buf, MAX_PATH) - 1;
    size += snprintf(buf + size, sizeof(buf) - size, "[TID: 0x%X] ", GetCurrentThreadId());

    va_list argptr;
    va_start(argptr, format);

    size += vsnprintf(buf + size, sizeof(buf) - size, format, argptr);
    WriteFile(file, buf, size, NULL, NULL);

    std::cout << "[LOG]";
    vprintf(format, argptr);

    va_end(argptr);
}

void Logger::Log(const std::string& message)
{
    //Logger::Log("%s\n", message.c_str());
    Logger::Log("%s", message.c_str());
}