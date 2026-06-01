#include <cstdint>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <windows.h>

#include <rex/ppc.h>

using RexPPCFunc = void (*)(PPCContext& ctx, std::uint8_t* base);

extern "C" RexPPCFunc daytona_resolve_import(
  const char* imp_name,
  const char* plain_name,
  const char* underscored_name
);

static void daytona_import_trace(const char* fmt, ...) {
  static const bool trace_enabled = std::getenv("DAYTONA_TRACE_IMPORTS") != nullptr;
  if (!trace_enabled) {
    return;
  }

  FILE* f = std::fopen("daytona_import_wrapper_trace.txt", "ab");
  if (!f) {
    return;
  }

  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(f, fmt, ap);
  va_end(ap);

  std::fclose(f);
}

static RexPPCFunc daytona_lookup_runtime_export(
  const char* imp_name,
  const char* plain_name,
  const char* underscored_name
) {
  void* p = nullptr;

  // Check the executable's own exports first. The per-game __imp__ overrides in
  // stubs.cpp (e.g. __imp__XamContentGetLicenseMask, __imp__VdInitializeRingBuffer)
  // are tagged with DAYTONA_EXPORT (__declspec(dllexport)), so they — and only
  // they — land in the EXE's export table and must win over the runtime DLL's
  // generic defaults. This mirrors the precedence Linux gets via whole-archive +
  // dlsym(RTLD_DEFAULT). Imports without an override aren't exported here, so they
  // miss and fall through to the DLL lookup below.
  HMODULE exe = GetModuleHandleA(nullptr);
  if (exe && imp_name) {
    p = reinterpret_cast<void*>(GetProcAddress(exe, imp_name));
  }
  if (!p && exe && plain_name) {
    p = reinterpret_cast<void*>(GetProcAddress(exe, plain_name));
  }
  if (!p && exe && underscored_name) {
    p = reinterpret_cast<void*>(GetProcAddress(exe, underscored_name));
  }

  HMODULE mod = nullptr;
  if (!p) {
    mod = GetModuleHandleA("librexruntime.dll");
    if (!mod) {
      mod = LoadLibraryA("librexruntime.dll");
    }

    if (mod && underscored_name) {
      p = reinterpret_cast<void*>(GetProcAddress(mod, underscored_name));
    }
    if (!p && mod && plain_name) {
      p = reinterpret_cast<void*>(GetProcAddress(mod, plain_name));
    }
    if (!p && mod && imp_name) {
      p = reinterpret_cast<void*>(GetProcAddress(mod, imp_name));
    }
  }

  daytona_import_trace(
    "lookup imp=%s plain=%s under=%s exe=%p mod=%p fn=%p\n",
    imp_name ? imp_name : "(null)",
    plain_name ? plain_name : "(null)",
    underscored_name ? underscored_name : "(null)",
    exe,
    mod,
    p
  );

  return reinterpret_cast<RexPPCFunc>(p);
}

extern "C" void daytona_call_import(
  const char* imp_name,
  const char* plain_name,
  const char* underscored_name,
  PPCContext& ctx,
  std::uint8_t* base
) {
  RexPPCFunc fn = daytona_resolve_import(imp_name, plain_name, underscored_name);

  daytona_import_trace(
    "call %s fn=%p lr=%08X r3=%08X r4=%08X r5=%08X r6=%08X\n",
    imp_name ? imp_name : "(null)",
    reinterpret_cast<void*>(fn),
    static_cast<unsigned>(ctx.lr & 0xFFFFFFFFu),
    ctx.r3.u32,
    ctx.r4.u32,
    ctx.r5.u32,
    ctx.r6.u32
  );

  if (fn) {
    fn(ctx, base);
    return;
  }

  daytona_import_trace("missing import %s; returning r3=0\n", imp_name ? imp_name : "(null)");
  ctx.r3.u64 = 0;
}

extern "C" RexPPCFunc daytona_resolve_import(
  const char* imp_name,
  const char* plain_name,
  const char* underscored_name
) {
  static std::mutex cache_mutex;
  static std::unordered_map<std::string, RexPPCFunc> cache;

  RexPPCFunc fn = nullptr;
  const char* key = underscored_name ? underscored_name : (plain_name ? plain_name : imp_name);

  {
    std::lock_guard<std::mutex> lock(cache_mutex);

    auto it = cache.find(key ? key : "");
    if (it == cache.end()) {
      fn = daytona_lookup_runtime_export(imp_name, plain_name, underscored_name);
      cache.emplace(key ? key : "", fn);
    } else {
      fn = it->second;
    }
  }

  return fn;
}
