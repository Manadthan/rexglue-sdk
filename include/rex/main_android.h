/**
 * @file        main_android.h
 * @brief       Android platform helpers (API level query, subsystem init).
 *
 * The SDL3 Java activity owns the Android lifecycle, so this is much thinner
 * than Xenia's main_android: it only exposes what core code queries.
 *
 * @license     BSD 3-Clause License
 */

#pragma once

#include <rex/platform.h>

#if REX_PLATFORM_ANDROID

namespace rex {

// Device API level (android_get_device_api_level), cached after first call.
int GetAndroidApiLevel();

// Initializes the Android parts of core (shared memory, thread names).
// Call once from the process entry point, before the runtime starts.
void InitializeAndroidCore();
void ShutdownAndroidCore();

}  // namespace rex

#endif  // REX_PLATFORM_ANDROID
