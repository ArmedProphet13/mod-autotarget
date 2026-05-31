// Stubs for production externs the off-client test binary doesn't link.
// HardTargetHandler logs on commit; the Logger implementation is Windows-bound
// so we provide a no-op here.

#include "Diagnostics/Logger.h"
#include "GameInterface/FrameScript.h"

namespace autotarget {

void Logger::Init(const std::string&, LogLevel) {}
void Logger::Shutdown() {}
void Logger::SetLevel(LogLevel) {}
void Logger::Write(LogLevel, const char*, ...) {}
LogLevel Logger::ParseLevel(const std::string&) { return LogLevel::Info; }

namespace FrameScript {
void Execute(const char*) {}
bool RegisterFunction(const char*, LuaCFunction) { return false; }
} // namespace FrameScript

} // namespace autotarget
