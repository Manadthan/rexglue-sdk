/**
 * @file        ui/windowed_app_main_sdl.cpp
 * @brief       Entry point for windowed applications (SDL3 windowing)
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/platform.h>
#include <rex/ui/windowed_app.h>
#include <rex/ui/windowed_app_context_sdl.h>

#if REX_PLATFORM_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#endif

#if REX_PLATFORM_ANDROID
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_system.h>

#include <rex/main_android.h>
#endif

namespace {

int RunWindowedApp(int argc, char** argv) {
  auto remaining = rex::cvar::Init(argc, argv);
  rex::cvar::ApplyEnvironment();
  rex::InitLoggingEarly();

  int result;
  {
    rex::ui::SDLWindowedAppContext app_context;
    if (!app_context.Initialize()) {
      return EXIT_FAILURE;
    }

#if REX_PLATFORM_WIN32
    // Apartment-threaded COM for shell dialogs.
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
      return EXIT_FAILURE;
    }
#endif

    std::unique_ptr<rex::ui::WindowedApp> app = rex::ui::GetWindowedAppCreator()(app_context);

    // Match remaining positional args to the app's expected options.
    const auto& option_names = app->GetPositionalOptions();
    std::map<std::string, std::string> parsed;
    size_t count = std::min(remaining.size(), option_names.size());
    for (size_t i = 0; i < count; ++i) {
      parsed[option_names[i]] = remaining[i];
    }
    app->SetParsedArguments(std::move(parsed));

    result = app->OnInitialize() ? app_context.RunMainMessageLoop() : EXIT_FAILURE;

    app->InvokeOnDestroy();
  }

#if REX_PLATFORM_WIN32
  CoUninitialize();
#endif

  return result;
}

#if REX_PLATFORM_WIN32
// Convert wide argv from CommandLineToArgvW to UTF-8 for cvar::Init.
std::vector<std::string> WideArgsToUtf8(int argc, wchar_t** wargv) {
  std::vector<std::string> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    std::wstring wide(wargv[i]);
    if (wide.empty()) {
      args.emplace_back();
      continue;
    }
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr,
                                   0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size,
                        nullptr, nullptr);
    args.push_back(std::move(utf8));
  }
  return args;
}
#endif

}  // namespace

#if REX_PLATFORM_WIN32

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE hinstance_prev, LPWSTR command_line,
                    int show_cmd) {
  (void)hinstance;
  (void)hinstance_prev;
  (void)command_line;
  (void)show_cmd;

  int wargc = 0;
  wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
  auto utf8_args = WideArgsToUtf8(wargc, wargv);
  LocalFree(wargv);

  std::vector<char*> argv_ptrs;
  argv_ptrs.reserve(utf8_args.size());
  for (auto& s : utf8_args) {
    argv_ptrs.push_back(s.data());
  }
  return RunWindowedApp(static_cast<int>(argv_ptrs.size()), argv_ptrs.data());
}

#elif REX_PLATFORM_ANDROID

namespace {

// stdout/stderr go nowhere on Android; forward them to logcat so the SDK's
// console log sink and fprintf diagnostics show up under the "rexglue" tag.
// Copy of everything written to stdout/stderr, flushed line by line, so a run
// that dies leaves its last words on disk (<app folder>/last_run.log) for the
// launcher's crash screen to show -- without a PC or adb.
FILE* g_run_log = nullptr;

void* LogcatPump(void* fd_ptr) {
  int fd = static_cast<int>(reinterpret_cast<intptr_t>(fd_ptr));
  char buf[1024];
  std::string line;
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; ++i) {
      if (buf[i] == '\n') {
        __android_log_write(ANDROID_LOG_INFO, "rexglue", line.c_str());
        if (g_run_log) {
          std::fputs(line.c_str(), g_run_log);
          std::fputc('\n', g_run_log);
          std::fflush(g_run_log);
        }
        line.clear();
      } else {
        line.push_back(buf[i]);
      }
    }
  }
  return nullptr;
}

void RedirectStdioToLogcat(const std::string& app_dir) {
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  if (!app_dir.empty()) {
    g_run_log = std::fopen((app_dir + "/last_run.log").c_str(), "w");
  }
  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) {
    return;
  }
  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  dup2(pipe_fds[1], STDOUT_FILENO);
  dup2(pipe_fds[1], STDERR_FILENO);
  pthread_t thread;
  if (pthread_create(&thread, nullptr, LogcatPump,
                     reinterpret_cast<void*>(intptr_t(pipe_fds[0]))) == 0) {
    pthread_detach(thread);
  }
}

}  // namespace

// SDL3's SDLActivity loads libmain.so and calls SDL_main on its own thread.
// Arguments come from SDLActivity.getArguments(); paths default to the app's
// external files folder (/sdcard/Android/data/<package>/files), which the user
// can reach over USB or with a file manager to drop the game data in.
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char* argv[]) {
  std::string app_dir;
  if (const char* external = SDL_GetAndroidExternalStoragePath()) {
    app_dir = external;
  } else if (const char* internal = SDL_GetAndroidInternalStoragePath()) {
    app_dir = internal;
  }
  RedirectStdioToLogcat(app_dir);
  std::printf("SDL_main: start, app folder %s, page size %d\n", app_dir.c_str(), getpagesize());

  // Recompiled code addresses the guest's physical window (0xE0000000+)
  // without the 4 KiB host offset that larger pages would need (see
  // REX_PHYS_HOST_OFFSET in the codegen init header, 0 on Linux/Android).
  // Kernels built for 16 KiB pages cannot run it, so say so instead of
  // corrupting memory.
  if (getpagesize() != 4096) {
    __android_log_print(ANDROID_LOG_FATAL, "rexglue", "unsupported page size %d (4096 needed)",
                        getpagesize());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unsupported device",
                             "This device uses memory pages larger than 4 KiB, which this "
                             "port does not support.",
                             nullptr);
    return EXIT_FAILURE;
  }

  rex::InitializeAndroidCore();

  if (!app_dir.empty()) {
    setenv("HOME", app_dir.c_str(), 1);
    setenv("XDG_DATA_HOME", app_dir.c_str(), 1);
    setenv("REX_ANDROID_APP_DIR", app_dir.c_str(), 1);
  }
  __android_log_print(ANDROID_LOG_INFO, "rexglue", "app folder: %s", app_dir.c_str());

  std::vector<std::string> args;
  args.emplace_back(argc > 0 && argv[0] ? argv[0] : "rexglue");
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  std::vector<char*> argv_ptrs;
  argv_ptrs.reserve(args.size());
  for (auto& s : args) {
    argv_ptrs.push_back(s.data());
  }
  std::printf("SDL_main: starting the app\n");
  int result = RunWindowedApp(static_cast<int>(argv_ptrs.size()), argv_ptrs.data());
  std::printf("SDL_main: app returned %d\n", result);
  rex::ShutdownAndroidCore();
  return result;
}

#else

int main(int argc, char* argv[]) {
  return RunWindowedApp(argc, argv);
}

#endif
