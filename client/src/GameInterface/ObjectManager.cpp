#include "GameInterface/ObjectManager.h"

#include "GameInterface/Memory.h"
#include "GameInterface/Offsets.h"

namespace autotarget {

namespace {
// Hard cap on the list walk so corrupt memory cannot spin forever.
constexpr int kMaxObjects = 16384;
} // namespace

bool ObjectManager::Refresh() {
    objMgr_ = 0;
    localGuid_ = kNoGuid;

    const std::uintptr_t conn = mem::ReadPtr(offsets::kStaticClientConnection);
    if (conn == 0)
        return false;
    const std::uintptr_t mgr = mem::ReadPtr(conn + offsets::kObjectManagerOffset);
    if (mgr == 0)
        return false;

    objMgr_ = mgr;
    localGuid_ = mem::ReadOr<Guid>(mgr + offsets::kLocalGuidOffset, kNoGuid);
    return localGuid_ != kNoGuid;
}

void ObjectManager::ForEachUnit(const std::function<void(const WoWUnit&)>& fn) const {
    if (objMgr_ == 0)
        return;

    std::uintptr_t cur = mem::ReadPtr(objMgr_ + offsets::kFirstObjectOffset);
    int guard = 0;
    while (cur != 0 && (cur & 1) == 0 && guard++ < kMaxObjects) {
        const WoWUnit unit(cur);
        if (unit.IsUnit())
            fn(unit);
        cur = mem::ReadPtr(cur + offsets::kNextObjectOffset);
    }
}

WoWUnit ObjectManager::FindUnit(Guid guid) const {
    WoWUnit found;
    if (guid == kNoGuid)
        return found;
    ForEachUnit([&](const WoWUnit& u) {
        if (!found.Valid() && u.ObjectGuid() == guid)
            found = u;
    });
    return found;
}

WoWUnit ObjectManager::LocalPlayer() const {
    return FindUnit(localGuid_);
}

} // namespace autotarget
