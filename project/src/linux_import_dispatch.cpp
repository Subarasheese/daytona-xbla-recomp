#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#include <dlfcn.h>

#include <rex/ppc.h>

using RexPPCFunc = void (*)(PPCContext& ctx, std::uint8_t* base);

extern "C" RexPPCFunc daytona_resolve_import(
    const char* imp_name,
    const char* plain_name,
    const char* underscored_name);

static std::mutex g_daytona_import_mutex;
static std::unordered_map<std::string, RexPPCFunc> g_daytona_import_cache;

static void daytona_trace_linux_import(const char* msg) {
  static const bool trace_enabled = std::getenv("DAYTONA_TRACE_IMPORTS") != nullptr;
  if (!trace_enabled) {
    return;
  }

  FILE* f = std::fopen("daytona_import_wrapper_trace.txt", "ab");
  if (f) {
    std::fprintf(f, "%s\n", msg);
    std::fclose(f);
  }
}

static RexPPCFunc daytona_lookup_linux_import_name(const char* name) {
  if (!name || !*name) {
    return nullptr;
  }

  void* p = dlsym(RTLD_DEFAULT, name);

  char msg[256];
  std::snprintf(msg, sizeof(msg), "linux lookup %s fn=%p", name, p);
  daytona_trace_linux_import(msg);

  return reinterpret_cast<RexPPCFunc>(p);
}

static RexPPCFunc daytona_lookup_linux_import(
    const char* imp_name,
    const char* plain_name,
    const char* underscored_name) {
  const char* key = underscored_name ? underscored_name : (plain_name ? plain_name : imp_name);
  if (!key) {
    return nullptr;
  }

  {
    std::lock_guard<std::mutex> lock(g_daytona_import_mutex);
    auto it = g_daytona_import_cache.find(key);
    if (it != g_daytona_import_cache.end()) {
      return it->second;
    }
  }

  RexPPCFunc fn = nullptr;
  fn = fn ? fn : daytona_lookup_linux_import_name(underscored_name);
  fn = fn ? fn : daytona_lookup_linux_import_name(plain_name);
  fn = fn ? fn : daytona_lookup_linux_import_name(imp_name);

  {
    std::lock_guard<std::mutex> lock(g_daytona_import_mutex);
    g_daytona_import_cache[key] = fn;
  }

  return fn;
}

extern "C" void daytona_call_import(
    const char* imp_name,
    const char* plain_name,
    const char* underscored_name,
    PPCContext& ctx,
    std::uint8_t* base) {
  RexPPCFunc fn = daytona_resolve_import(imp_name, plain_name, underscored_name);

  if (fn) {
    fn(ctx, base);
    return;
  }

  char msg[512];
  std::snprintf(
    msg,
    sizeof(msg),
    "linux missing import imp=%s plain=%s underscored=%s lr=%08X",
    imp_name ? imp_name : "(null)",
    plain_name ? plain_name : "(null)",
    underscored_name ? underscored_name : "(null)",
    static_cast<unsigned>(ctx.lr & 0xFFFFFFFFu)
  );
  daytona_trace_linux_import(msg);

  ctx.r3.u64 = 0;
}

extern "C" RexPPCFunc daytona_resolve_import(
    const char* imp_name,
    const char* plain_name,
    const char* underscored_name) {
  return daytona_lookup_linux_import(imp_name, plain_name, underscored_name);
}
