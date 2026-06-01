set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)

set(CMAKE_C_COMPILER_TARGET x86_64-w64-windows-gnu)
set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-windows-gnu)

# Match the Linux preset's ISA baseline (-march=x86-64-v3 → AVX2/FMA/BMI2).
# x86-64-v3 is a strict superset of SSSE3; the recompiled VMX/AltiVec emulation
# (via simde) and the whole runtime DLL are far slower when capped at SSSE3.
set(CMAKE_C_FLAGS_INIT "--target=x86_64-w64-windows-gnu -I${CMAKE_CURRENT_LIST_DIR}/mingw-compat -march=x86-64-v3 -fms-extensions")
set(CMAKE_CXX_FLAGS_INIT "--target=x86_64-w64-windows-gnu -I${CMAKE_CURRENT_LIST_DIR}/mingw-compat -march=x86-64-v3 -fms-extensions")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld")

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
