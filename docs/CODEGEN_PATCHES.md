# Codegen Patch Notes

This repository keeps a small manual patch on top of ReXGlue-generated output.
The patch is needed after regenerating code because ReXGlue still leaves a few
Daytona branch targets unresolved.

## Regenerate

Use the CMake target instead of a hand-written local command:

```sh
cmake --preset linux-amd64 -S project
cmake --build project/out/build/linux-amd64 --config Release --target daytona_codegen
```

On this checkout, codegen can print `Done` and still end with a signal 11 or
code 139 failure from the generator process. If the generated files were written,
apply the patch before building the game:

```sh
patch -p0 < patches/daytona_working_codegen.patch
```

## Patch Contents

`patches/daytona_working_codegen.patch` reapplies the local generated-code fixes
that are not produced by ReXGlue yet.

It adds declarations and registration entries for:

```text
sub_82342258
sub_824C8D80
```

It also replaces selected generated `REX_FATAL("Unresolved branch ...")`
fallbacks with direct `goto loc_...` branches in:

```text
generated/daytona_recomp.10.cpp
generated/daytona_recomp.19.cpp
generated/daytona_recomp.26.cpp
```

## Local Symbol Header

`project/src/daytona_symbols.h` maps generated `sub_XXXXXXXX` names to local
symbolic aliases used by `project/src/stubs.cpp`.
