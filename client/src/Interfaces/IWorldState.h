#pragma once

namespace autotarget {

// Narrow interface for "is the player in the world right now?". The
// Bootstrap layer gates UI install on this so the in-game checkbox is
// only created once FrameXML is loaded.
class IWorldState {
public:
    virtual ~IWorldState() = default;

    virtual bool InWorld() const = 0;
};

} // namespace autotarget
