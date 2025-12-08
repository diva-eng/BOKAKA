#pragma once

// ============================
// Firmware Version Definition
// ============================

// Major version number (increment on breaking changes)
#define FW_VERSION_MAJOR 1

// Minor version (add new features)
#define FW_VERSION_MINOR 0

// Patch version (bug fixes)
#define FW_VERSION_PATCH 0

// String form
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define FW_VERSION_STRING  \
    STR(FW_VERSION_MAJOR) "." STR(FW_VERSION_MINOR) "." STR(FW_VERSION_PATCH)


// ============================
// Build Metadata
// ============================

// Auto-insert build date/time (gcc predefined macros)
#define FW_BUILD_DATE __DATE__
#define FW_BUILD_TIME __TIME__

// Optional git hash (you can have PlatformIO inject this later)
#ifndef FW_BUILD_HASH
#define FW_BUILD_HASH "dev"
#endif
