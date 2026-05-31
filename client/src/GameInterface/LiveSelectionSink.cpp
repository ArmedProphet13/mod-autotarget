#include "GameInterface/LiveSelectionSink.h"

#include "GameInterface/Selection.h"
#include "GameInterface/Hooks/SelectionHook.h"

namespace autotarget {

Guid LiveSelectionSink::ReadMouseover() { return Selection::Mouseover(); }
void LiveSelectionSink::WriteMouseover(Guid g) { Selection::SetMouseover(g); }
void LiveSelectionSink::BeginActiveTargetCommit() { SelectionHook::BeginCommit(); }
void LiveSelectionSink::EndActiveTargetCommit() { SelectionHook::EndCommit(); }
void LiveSelectionSink::WriteActiveTarget(Guid g) { Selection::SetTarget(g); }

} // namespace autotarget
