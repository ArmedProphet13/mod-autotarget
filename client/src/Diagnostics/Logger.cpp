#include "Logger.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace autotarget {

namespace {

std::mutex g_mutex;
FILE*      g_file = nullptr;
LogLevel   g_level = LogLevel::Info;
bool       g_initialized = false;

const char* LevelName(LogLevel l) {
    switch (l) {
        case LogLevel::Error: return "ERROR";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Debug: return "DEBUG";
    }
    return "?????";
}

} // namespace

void Logger::Init(const std::string& filePath, LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
    g_file = fopen(filePath.c_str(), "w");
    g_level = level;
    g_initialized = (g_file != nullptr);
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_file) {
        fclose(g_file);
        g_file = nullptr;
    }
    g_initialized = false;
}

void Logger::SetLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_level = level;
}

void Logger::Write(LogLevel level, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_initialized || !g_file)
        return;
    if (static_cast<int>(level) > static_cast<int>(g_level))
        return;

    SYSTEMTIME st;
    GetLocalTime(&st);

    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    fprintf(g_file, "[%02d:%02d:%02d.%03d] %s  %s\n",
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            LevelName(level), msg);
    fflush(g_file);
}

LogLevel Logger::ParseLevel(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (n == "error") return LogLevel::Error;
    if (n == "warn")  return LogLevel::Warn;
    if (n == "debug") return LogLevel::Debug;
    return LogLevel::Info;
}

} // namespace autotarget
