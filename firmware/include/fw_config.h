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

// Build datetime injected by extra_script.py (ISO 8601 UTC format)
// Format: "2026-01-02T15:30:45Z"
#ifndef FW_BUILD_DATETIME
#define FW_BUILD_DATETIME "unknown"
#endif

// Git hash injected by extra_script.py
#ifndef FW_BUILD_HASH
#define FW_BUILD_HASH "dev"
#endif
