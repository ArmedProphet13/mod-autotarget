#pragma once

#include <cstdint>
#include <functional>

#include "Engine/EngineTypes.h"
#include "GameInterface/WoWObject.h"

namespace autotarget {

// Walks the client's object manager linked list and exposes the local player
// and the visible units.
class ObjectManager {
public:
    // Re-reads the live object-manager pointers. Returns false when the player
    // is not yet in the world.
    bool Refresh();

    bool InWorld() const { return objMgr_ != 0; }
    Guid LocalPlayerGuid() const { return localGuid_; }
    WoWUnit LocalPlayer() const;

    // Visits every unit/player object currently in the manager.
    void ForEachUnit(const std::function<void(const WoWUnit&)>& fn) const;

    // Resolves a GUID to its unit (invalid WoWUnit if not present).
    WoWUnit FindUnit(Guid guid) const;

private:
    std::uintptr_t objMgr_ = 0;
    Guid localGuid_ = kNoGuid;
};

} // namespace autotarget
