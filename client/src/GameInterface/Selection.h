#pragma once

#include <cstdint>

#include "Engine/EngineTypes.h"

namespace autotarget {

// Reads and writes the player's hard target through the client's own routine.
namespace Selection {

// Overrides the CGGameUI::Set_Mouseover address used for the native tooltip +
// highlight call. INI-driven so candidate offsets can be probed in-client
// without a rebuild. Pass 0 to disable the native call (raw slot write only).
// Call before the first SetMouseover (TickCoordinator ctor).
void SetMouseoverOffset(std::uintptr_t addr);

// Enables the Tier-2 fallback: after the raw slot write, drive the game's own
// GameTooltip via Lua (GameTooltip:SetUnit("mouseover")). Tooltip only, no ring.
void SetTooltipViaLua(bool on);

// The player's current hard target (kNoGuid if none).
Guid CurrentTarget();

// The unit currently under the cursor (kNoGuid if none).
Guid Mouseover();

// Overwrites the client's mouseover GUID slot. AutoTarget calls this every
// frame to pin the mouseover unit to its soft target, so /cast [@mouseover]
// macros and the mouseover highlight follow the soft target. Pass kNoGuid to
// release the slot back to the cursor.
void SetMouseover(Guid guid);

// Sets the player's hard target by calling CGGameUI::Target. The caller is
// responsible for bracketing this with SelectionHook::BeginCommit/EndCommit so
// the selection hook does not mistake it for a manual pick.
void SetTarget(Guid guid);

} // namespace Selection
} // namespace autotarget
