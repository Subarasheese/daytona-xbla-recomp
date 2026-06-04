# Daytona USA ReXGlue Recompilation

![Daytona Recompilation](daytonarecomp.png)

This project is a static recompilation of Daytona USA (Xbox 360 / XBLA, 2011) using Rexglue.

No copyrighted game files are included. This tree excludes the
extracted game directory, `default.xex`, `pe_image.bin`, media archives, audio,
images, and build outputs.

## Prerequisites

### All platforms

- **CMake 3.25+**
- **Ninja** build system

### Windows

- [**Visual Studio 2022 Community**](https://visualstudio.microsoft.com/vs/community/) with the **Desktop development with C++** workload, including:
  - **C++ Clang Compiler for Windows** (20.x or newer)
  - **MSBuild support for LLVM (clang-cl) toolset**

### Linux

- **Clang 20+**
- GTK3 development headers (`libgtk-3-dev` on Debian/Ubuntu, `gtk3` on Arch)
- X11/XCB interop library (`libx11-xcb-dev` on Debian/Ubuntu, `libx11` on Arch, `libX11-devel` on Fedora)
- XWayland or a native X11 session (the SDK renders via XCB; on Wayland, XWayland must be running)

## Included

```text
config/                         ReXGlue manifest/config files
patches/daytona_working_codegen.patch
docs/CODEGEN_PATCHES.md         notes for the manual codegen patch
ppc/                            PPC metadata headers
project/                        host project sources and CMake files
scripts/extract_game.py         local STFS package extractor
thirdparty/rexglue-sdk           required ReXGlue SDK submodule
```

The host project sources were copied from the running `daytona_working` tree,
including:

```text
project/src/daytona_symbols.h
project/src/stubs.cpp
project/src/main.cpp
```

`project/src/daytona_symbols.h` is the full symbol map from the working version.

## Required local game files

To regenerate codegen or run the project, provide your own legally obtained
Daytona USA XBLA package in:

```text
game/
```

The local package currently used with this tree is:

```text
game/{game_file_with_id}
type: Microsoft Xbox 360 LIVE/STFS package, Arcade Title
title/media: XA-2845, media ID 3CA562D4
size: 240058368 bytes
sha256: db2381451a15a4e537154712213a0309e9ddc33d20bdc03421d80c9939893cef
```

Extract it from the repository root with:

```sh
python3 scripts/extract_game.py
```

The script extracts the STFS package into:

```text
extracted/
```

and copies the executable required by ReXGlue to:

```text
assets/default.xex
```

`config/daytona_manifest.toml` references that executable as:

```text
../assets/default.xex
```

`game/`, `extracted/`, and `assets/` are ignored by git because they contain
copyrighted game files. A successful extraction currently produces 162 files;
the root `default.xex` should be detected by `file` as:

```text
Microsoft Xbox 360 executable (XA-2845, media ID: 3CA562D4), all regions
```

## Required ReXGlue SDK

This recompilation requires the `daytonaxbla` branch of the ReXGlue SDK — **not** the default branch. It is vendored as a git submodule at:

```text
thirdparty/rexglue-sdk
```

After cloning this repository, initialize it with:

```sh
git submodule update --init --recursive
```

If `git submodule status --recursive` prints nothing even though `.gitmodules`
contains `thirdparty/rexglue-sdk`, the submodule was not recorded in the index.
Re-add it — the `-b daytonaxbla` flag is required:

```sh
git submodule add -b daytonaxbla https://github.com/Subarasheese/rexglue-sdk.git thirdparty/rexglue-sdk
git submodule update --init --recursive
```

The current submodule checkout used when this tree was prepared is:

```text
82a88a385c456ccfd82eae2b320955d0f028d92d
```

CMake may warn that no `v*` tag is reachable from the SDK checkout and fall
back to `0.8.0.0-dev.unknown`. That warning did not block configure, codegen, or
the Release build on this tree.

## Regenerate codegen

From the repository root:

**Linux**

```sh
cmake --preset linux-amd64 -S project
cmake --build project/out/build/linux-amd64 --config Release --target daytona_codegen
```

**Windows**

```bat
cmake --preset win-amd64 -S project
cmake --build project/out/build/win-amd64 --config Release --target daytona_codegen
```

That target uses the `rex::rexglue` executable built from
`thirdparty/rexglue-sdk`. The `generated/` and `config/generated/` directories
are local codegen outputs and are intentionally ignored by Git.

On this checkout, the `daytona_codegen` target successfully regenerated files
and printed `Done in 4.4s.`, then `rexglue` exited with signal 11 and Ninja
reported the target as failed. Treat the generated files as usable if the run
reaches `Done`, then apply the working patch below.

## Apply working codegen fixes

The working daytona tree has manual fixes on top of regenerated
codegen. Apply them with:

```sh
patch -p0 < patches/daytona_working_codegen.patch
```

The patch changes only generated files. Keep the generated outputs ignored and
commit the patch, not the regenerated files. See `docs/CODEGEN_PATCHES.md` for
the exact files and reasons.

## Build

> **Prerequisites:** complete [Regenerate codegen](#regenerate-codegen) and [Apply working codegen fixes](#apply-working-codegen-fixes) first. The build requires generated headers such as `daytona_init.h` that do not exist in the repository.

Configure and build from the repository root.

**Linux**

```sh
cmake --preset linux-amd64 -S project
cmake --build project/out/build/linux-amd64 --config Release
```

**Windows**

```bat
cmake --preset win-amd64 -S project
cmake --build project/out/build/win-amd64 --config Release
```

Alternatively on either platform, `cd project/` first and run the presets without the `-S` / path overrides:

```sh
# Linux
cd project
cmake --preset linux-amd64
cmake --build --preset linux-amd64-release
```

```bat
rem Windows
cd project
cmake --preset win-amd64
cmake --build --preset win-amd64-release
```

If configuration fails with a missing SDK message, run:

```sh
git submodule update --init --recursive
```

The Daytona target directly compiles SDK sources including `rex_app.cpp`, so it
needs two SDK include paths in `project/CMakeLists.txt`:

```text
thirdparty/rexglue-sdk/thirdparty/imgui
project/out/build/<preset>/rexglue-sdk/include   (linux-amd64 or win-amd64)
```

Without them, the Release build fails with missing `imgui.h` or
`rex/version.h`.

## Run

Provide the extracted game directory via `--game_data_root`.

**Linux**

The executable links against SDK shared libraries from the build output. Run it
with `LD_LIBRARY_PATH`:

```sh
LD_LIBRARY_PATH="$PWD/thirdparty/rexglue-sdk/out/linux-amd64/Release" \
  project/out/build/linux-amd64/Release/daytona \
  --game_data_root="$PWD/extracted"
```

For a short smoke test that does not leave the process running:

```sh
mkdir -p logs
timeout 20 env \
  DISPLAY=:1 \
  GDK_BACKEND=x11 \
  LD_LIBRARY_PATH="$PWD/thirdparty/rexglue-sdk/out/linux-amd64/Release" \
  project/out/build/linux-amd64/Release/daytona \
  --game_data_root="$PWD/extracted" \
  --log_file="$PWD/logs/daytona_run.log" \
  --log_level=debug
```

If `LD_LIBRARY_PATH` is omitted, `ldd project/out/build/linux-amd64/Release/daytona`
shows `librexruntime.so => not found` and `libTracyClient.so => not found`.

**Windows**

The build step automatically copies runtime DLLs next to the executable, so no
`PATH` changes are needed. Run from the repository root:

```bat
project\out\build\win-amd64\Release\daytona.exe --game_data_root=extracted
```

`logs/` and `*.log` are ignored so local runtime logs are not uploaded.

This has been confirmed to build and run from this checkout after extracting the
package, regenerating code, applying `patches/daytona_working_codegen.patch`,
and building the Release preset. During a working run, the log may still print
repeated `NtQueryInformationFile(XFileXctdCompressionInformation) unimplemented`
messages and occasional `Skipping Vulkan frame presentation due to async
placeholder draw usage in this frame` warnings.

If `--game_data_root` is omitted, startup exits immediately with:

```text
[ERROR] --game_data_root was not provided.
```
