/**
 * @file        rex/system/shared_library.h
 * @brief       Platform-agnostic shared library loader
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#pragma once

#include <string>

namespace rex::system {

class SharedLibrary {
 public:
  SharedLibrary() = default;
  ~SharedLibrary();

  SharedLibrary(const SharedLibrary&) = delete;
  SharedLibrary& operator=(const SharedLibrary&) = delete;
  SharedLibrary(SharedLibrary&& other) noexcept;
  SharedLibrary& operator=(SharedLibrary&& other) noexcept;

  bool Load(const std::string& name);
  void* GetSymbol(const char* name);
  void Close();
  bool is_loaded() const { return handle_ != nullptr; }

 private:
  void* handle_ = nullptr;
};

}  // namespace rex::system
