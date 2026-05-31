#pragma once

#include <string>

namespace autotarget {

enum class LogLevel {
    Error = 0,
    Warn  = 1,
    Info  = 2,
    Debug = 3
};

// Leveled file logger. AutoTarget runs hands-off inside WoW.exe with no debugger
// attached, so the log file is the primary diagnostic channel. Thread-safe.
class Logger {
public:
    static void Init(const std::string& filePath, LogLevel level);
    static void Shutdown();
    static void SetLevel(LogLevel level);
    static void Write(LogLevel level, const char* fmt, ...);

    // Parse a level name ("error" / "warn" / "info" / "debug"); defaults to Info.
    static LogLevel ParseLevel(const std::string& name);

private:
    Logger() = default;
};

} // namespace autotarget

#define AT_LOG_ERROR(...) ::autotarget::Logger::Write(::autotarget::LogLevel::Error, __VA_ARGS__)
#define AT_LOG_WARN(...)  ::autotarget::Logger::Write(::autotarget::LogLevel::Warn,  __VA_ARGS__)
#define AT_LOG_INFO(...)  ::autotarget::Logger::Write(::autotarget::LogLevel::Info,  __VA_ARGS__)
#define AT_LOG_DEBUG(...) ::autotarget::Logger::Write(::autotarget::LogLevel::Debug, __VA_ARGS__)
