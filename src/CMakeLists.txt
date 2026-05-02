cmake_minimum_required(VERSION 3.10)
project(umcx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../bin")

# ---------------------------------------------------------------------------
# Backend selection  (mirrors Makefile targets)
#   OMP          - OpenMP CPU parallel, offload disabled        (default/multi)
#   SINGLE       - single-threaded CPU, no OpenMP
#   NVIDIA       - GCC OpenMP offload to NVIDIA GPU             (nvidia)
#   NVIDIA_CLANG - Clang/LLVM OpenMP offload to NVIDIA GPU      (nvidiaclang)
#   AMD          - GCC OpenMP offload to AMD GPU                (amd)
#   AMD_CLANG    - Clang/LLVM OpenMP offload to AMD GPU         (amdclang)
#   NVC          - NVIDIA nvc++ OpenMP/OpenACC GPU offload      (nvc)
# ---------------------------------------------------------------------------
set(BACKEND "OMP" CACHE STRING
    "GPU/threading backend: OMP SINGLE NVIDIA NVIDIA_CLANG AMD AMD_CLANG NVC")
set_property(CACHE BACKEND PROPERTY STRINGS
    OMP SINGLE NVIDIA NVIDIA_CLANG AMD AMD_CLANG NVC)

option(ACC   "Use OpenACC instead of OpenMP for offload/CPU backends" OFF)
option(DEBUG "Enable DEBUG preprocessor define"                      OFF)

# Path to CUDA installation, needed by NVIDIA_CLANG (--cuda-path=...)
set(CUDA_PATH "" CACHE PATH "CUDA installation path (NVIDIA_CLANG backend)")

# AMD GPU architecture (e.g. gfx906, gfx1030, gfx1100); run: rocminfo | grep gfx
set(GFX "gfx906" CACHE STRING "AMD GPU architecture for AMD/AMD_CLANG backends")
# NVIDIA GPU architecture for NVIDIA_CLANG (e.g. sm_50, sm_70, sm_86)
set(SM  "sm_50"  CACHE STRING "NVIDIA GPU SM version for NVIDIA_CLANG backend")
# nvc++ GPU targets: comma-separated cc values + optional ptx for JIT forward compat
set(CC_ARCH "cc70,cc80,cc86,cc90,ptx" CACHE STRING "nvc++ GPU architectures for NVC backend (e.g. cc80,cc90,ptx or ptx)")

# ---------------------------------------------------------------------------
add_executable(umcx umcx.cpp)
target_include_directories(umcx PRIVATE .)
target_compile_options(umcx PRIVATE -g -Wall -Wextra -pedantic -O3)

if(DEBUG)
    target_compile_definitions(umcx PRIVATE DEBUG)
endif()

if(BACKEND MATCHES "^(NVIDIA|NVIDIA_CLANG|AMD|AMD_CLANG|NVC)$")
    target_compile_definitions(umcx PRIVATE GPU_OFFLOAD)
endif()

# Resolve -fopenacc vs -fopenmp for non-NVC backends
if(ACC AND NOT BACKEND STREQUAL "NVC" AND NOT BACKEND STREQUAL "SINGLE")
    set(_ompacc "-fopenacc")
else()
    set(_ompacc "-fopenmp")
endif()

# ---------------------------------------------------------------------------
set(_link_flags "")

if(BACKEND STREQUAL "SINGLE")
    # CPU single-thread, no extra flags

elseif(BACKEND STREQUAL "OMP")
    if(ACC)
        target_compile_options(umcx PRIVATE ${_ompacc} -Wno-unknown-pragmas)
        set(_link_flags "${_ompacc}")
    else()
        find_package(OpenMP REQUIRED)
        target_compile_options(umcx PRIVATE ${OpenMP_CXX_FLAGS})
        set(_link_flags "${OpenMP_CXX_FLAGS}")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(umcx PRIVATE -foffload=disable)
            set(_link_flags "${_link_flags} -foffload=disable")
        endif()
    endif()

elseif(BACKEND STREQUAL "NVIDIA")
    target_compile_options(umcx PRIVATE
        ${_ompacc} -foffload=nvptx-none -foffload=-lm
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "${_ompacc} -foffload=nvptx-none")

elseif(BACKEND STREQUAL "NVIDIA_CLANG")
    target_compile_options(umcx PRIVATE
        ${_ompacc} -fopenmp-targets=nvptx64 --cuda-gpu-arch=${SM} -Wno-openmp-mapping)
    if(CUDA_PATH)
        target_compile_options(umcx PRIVATE --cuda-path=${CUDA_PATH})
    endif()
    set(_link_flags "${_ompacc}")

elseif(BACKEND STREQUAL "AMD")
    target_compile_options(umcx PRIVATE
        ${_ompacc} -foffload=amdgcn-amdhsa=-march=${GFX} -foffload=-lm
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "${_ompacc} -foffload=amdgcn-amdhsa=-march=${GFX}")

elseif(BACKEND STREQUAL "AMD_CLANG")
    target_compile_options(umcx PRIVATE
        -target x86_64-pc-linux-gnu
        ${_ompacc} -fopenmp-targets=amdgcn-amd-amdhsa
        -Xopenmp-target=amdgcn-amd-amdhsa -march=${GFX}
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "${_ompacc}")

elseif(BACKEND STREQUAL "NVC")
    if(ACC)
        set(_nvc_ompacc "-acc=gpu")
    else()
        set(_nvc_ompacc "-mp=gpu")
    endif()
    target_compile_options(umcx PRIVATE
        -Minfo=mp,accel -Minline
        "-gpu=mem:managed,${CC_ARCH}" -static-nvidia ${_nvc_ompacc})
    set(_link_flags "${_nvc_ompacc} -gpu=mem:managed,${CC_ARCH} -static-nvidia")

else()
    message(FATAL_ERROR "Unknown BACKEND '${BACKEND}'. "
        "Valid values: OMP SINGLE NVIDIA NVIDIA_CLANG AMD AMD_CLANG NVC")
endif()

# Windows + GCC: statically link runtime to prevent DLL heap mismatch
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(_link_flags "${_link_flags} -static-libgcc -static-libstdc++")
endif()

if(_link_flags)
    set_target_properties(umcx PROPERTIES LINK_FLAGS "${_link_flags}")
endif()
