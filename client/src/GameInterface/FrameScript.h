#pragma once

namespace autotarget {

// Bridge to the client's Lua engine (FrameScript). Both calls run code on the
// game thread and MUST be invoked from the frame callback, never the init
// thread — the client's Lua state is not thread-safe.
namespace FrameScript {

// Runs a Lua string. Used to install the Interface Options checkbox and the
// /at slash command.
void Execute(const char* luaCode);

// Standard Lua C-function signature: int func(lua_State*). The state pointer is
// passed through untouched — AutoTarget's callbacks take no arguments and ignore
// it, which keeps the bridge free of any lua_to* offset dependencies.
using LuaCFunction = int(__cdecl*)(void* luaState);

// Exposes a native function to Lua under the given global name. Returns false
// when the client address is not configured (kFnFrameScriptRegisterFunction is
// 0) — in that case the hotkey remains the working toggle.
bool RegisterFunction(const char* name, LuaCFunction fn);

} // namespace FrameScript
} // namespace autotarget
