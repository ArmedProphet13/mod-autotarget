#pragma once

namespace autotarget {

// Narrow interface for the master on/off control. The in-game checkbox,
// /at slash command, and any future hotkey bind to this surface instead
// of the whole controller.
class IToggleTarget {
public:
    virtual ~IToggleTarget() = default;

    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool on) = 0;
};

} // namespace autotarget
