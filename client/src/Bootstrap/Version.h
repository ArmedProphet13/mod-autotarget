#pragma once

// Single source of truth for the AutoTarget version. Update on every release
// and tag the corresponding commit in git with the matching v<MAJOR>.<MINOR>.<PATCH>.
//
// The version is written to the top of every AutoTarget.log run so that bug
// reports can be unambiguously matched to a build.

#define AUTOTARGET_VERSION_MAJOR 0
#define AUTOTARGET_VERSION_MINOR 3
#define AUTOTARGET_VERSION_PATCH 5

#define AUTOTARGET_VERSION_STRING "0.3.5"
