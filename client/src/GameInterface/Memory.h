#pragma once

#include <cstdint>

namespace autotarget {
namespace mem {

// AutoTarget runs inside WoW.exe, so "reading client memory" is simply a guarded
// pointer dereference. The plausibility check rejects obviously bad pointers;
// genuine faults on a stale pointer are contained by SafeMode's structured-
// exception handler one level up.

inline bool Plausible(std::uintptr_t addr) {
    return addr >= 0x00010000 && addr < 0x7FFF0000;
}

template <typename T>
inline T ReadOr(std::uintptr_t addr, T fallback) {
    if (!Plausible(addr))
        return fallback;
    return *reinterpret_cast<T*>(addr);
}

inline std::uintptr_t ReadPtr(std::uintptr_t addr) {
    if (!Plausible(addr))
        return 0;
    return *reinterpret_cast<std::uintptr_t*>(addr);
}

template <typename T>
inline void Write(std::uintptr_t addr, T value) {
    if (Plausible(addr))
        *reinterpret_cast<T*>(addr) = value;
}

} // namespace mem
} // namespace autotarget
