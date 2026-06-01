/**
 * @file        rexglue/commands/init_command.h
 * @brief       Project initialization command interface
 *
 * @copyright   Copyright (c) 2026 Tom Clay
 * @license     BSD 3-Clause License
 */

#pragma once

#include "../cli_utils.h"

#include <string>

#include <rex/result.h>

namespace CLI {
class App;
}

namespace rexglue::cli {

using rex::Result;

struct InitOptions {
  std::string project_name;
  std::string xex_path;
  std::string game_root;
  std::string project_root;
  bool scan_dlls = false;
  std::string template_dir;
  bool force = false;
};

struct InitModuleOptions {
  std::string app_root;
  std::string xex_path;
  std::string guest_path;
};

Result<void> InitProject(const InitOptions& opts, const CliContext& ctx);
Result<void> InitModule(const InitModuleOptions& opts, const CliContext& ctx);

void RegisterInit(CLI::App& parent, const CliContext& ctx, DeferredAction& pending);

}  // namespace rexglue::cli
