/**
 * @file        system/shared_library.cpp
 * @brief       Platform-agnostic shared library loader
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/system/shared_library.h>

#include <fmt/format.h>

#include <rex/assert.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace rex::system {

namespace {

#ifdef _WIN32
std::string FormatLastError(DWORD err) {
  char* buffer = nullptr;
  DWORD len = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&buffer), 0,
      nullptr);
  std::string msg = (len && buffer) ? std::string(buffer, len) : std::string{};
  if (buffer) {
    LocalFree(buffer);
  }
  while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r' || msg.back() == ' ')) {
    msg.pop_back();
  }
  return msg.empty() ? fmt::format("error {}", err) : fmt::format("{} (error {})", msg, err);
}
#endif

}  // namespace

SharedLibrary::~SharedLibrary() {
  Close();
}

SharedLibrary::SharedLibrary(SharedLibrary&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

SharedLibrary& SharedLibrary::operator=(SharedLibrary&& other) noexcept {
  if (this != &other) {
    Close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

bool SharedLibrary::Load(const std::string& name) {
  assert_true(handle_ == nullptr);
  if (handle_) {
    REXSYS_ERROR("SharedLibrary::Load called over a live handle");
    return false;
  }
  auto exe_dir = rex::filesystem::GetExecutableFolder();
#ifdef _WIN32
  std::string full_name = (exe_dir / (name + ".dll")).string();
  handle_ = LoadLibraryA(full_name.c_str());
  if (!handle_) {
    DWORD err = GetLastError();
    REXSYS_ERROR("Failed to load shared library '{}': {}", full_name, FormatLastError(err));
    return false;
  }
#else
  std::string full_name = (exe_dir / ("lib" + name + ".so")).string();
  handle_ = dlopen(full_name.c_str(), RTLD_NOW);
  if (!handle_) {
    const char* err = dlerror();
    REXSYS_ERROR("Failed to load shared library '{}': {}", full_name, err ? err : "unknown");
    return false;
  }
#endif
  return true;
}

void* SharedLibrary::GetSymbol(const char* name) {
  if (!handle_)
    return nullptr;
#ifdef _WIN32
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), name));
#else
  return dlsym(handle_, name);
#endif
}

void SharedLibrary::Close() {
  if (!handle_) {
    return;
  }
#ifdef _WIN32
  if (!FreeLibrary(static_cast<HMODULE>(handle_))) {
    REXSYS_ERROR("FreeLibrary failed: {}", FormatLastError(GetLastError()));
  }
#else
  if (dlclose(handle_) != 0) {
    const char* err = dlerror();
    REXSYS_ERROR("dlclose failed: {}", err ? err : "unknown");
  }
#endif
  handle_ = nullptr;
}

}  // namespace rex::system
