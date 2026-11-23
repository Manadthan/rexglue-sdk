/**
 * @file        core/main_android.cpp
 * @brief       Android platform helpers, see rex/main_android.h.
 *
 * @license     BSD 3-Clause License
 */

#include <rex/main_android.h>

#include <android/api-level.h>

#include <string_view>

#include <rex/filesystem.h>
#include <rex/memory/utils.h>
#include <rex/system.h>
#include <rex/thread.h>

namespace rex {

int GetAndroidApiLevel() {
  static const int api_level = android_get_device_api_level();
  return api_level;
}

void InitializeAndroidCore() {
  memory::AndroidInitialize();
  thread::AndroidInitialize();
}

void ShutdownAndroidCore() {
  thread::AndroidShutdown();
  memory::AndroidShutdown();
}

bool InitializeAndroidSystemForApplicationContext() {
  return true;
}
void ShutdownAndroidSystem() {}

namespace filesystem {

void AndroidInitialize() {}
void AndroidShutdown() {}

bool IsAndroidContentUri(const std::string_view source) {
  return source.starts_with("content://");
}

// Content URIs need a JNI round trip through ContentResolver; the port only
// reads game data from plain paths in app storage, so these are refused.
int OpenAndroidContentFileDescriptor(const std::string_view, const char*) {
  return -1;
}

}  // namespace filesystem
}  // namespace rex
