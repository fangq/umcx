cmake_minimum_required(VERSION 3.5)
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

option(ACC   "Use OpenACC instead of OpenMP for the NVC backend" OFF)
option(DEBUG "Enable DEBUG preprocessor define"                  OFF)

# Path to CUDA installation, needed by NVIDIA_CLANG (--cuda-path=...)
set(CUDA_PATH "" CACHE PATH "CUDA installation path (NVIDIA_CLANG backend)")

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

# ---------------------------------------------------------------------------
set(_link_flags "")

if(BACKEND STREQUAL "SINGLE")
    # CPU single-thread, no extra flags

elseif(BACKEND STREQUAL "OMP")
    find_package(OpenMP REQUIRED)
    target_compile_options(umcx PRIVATE ${OpenMP_CXX_FLAGS})
    set(_link_flags "${OpenMP_CXX_FLAGS}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(umcx PRIVATE -foffload=disable)
        set(_link_flags "${_link_flags} -foffload=disable")
    endif()

elseif(BACKEND STREQUAL "NVIDIA")
    target_compile_options(umcx PRIVATE
        -fopenmp -foffload=nvptx-none -foffload=-lm
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "-fopenmp -foffload=nvptx-none")

elseif(BACKEND STREQUAL "NVIDIA_CLANG")
    target_compile_options(umcx PRIVATE
        -fopenmp -fopenmp-targets=nvptx64 --cuda-gpu-arch=sm_50)
    if(CUDA_PATH)
        target_compile_options(umcx PRIVATE --cuda-path=${CUDA_PATH})
    endif()
    set(_link_flags "-fopenmp")

elseif(BACKEND STREQUAL "AMD")
    target_compile_options(umcx PRIVATE
        -fopenmp -foffload=amdgcn-amdhsa=-march=gfx906 -foffload=-lm
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "-fopenmp -foffload=amdgcn-amdhsa=-march=gfx906")

elseif(BACKEND STREQUAL "AMD_CLANG")
    target_compile_options(umcx PRIVATE
        -target x86_64-pc-linux-gnu
        -fopenmp -fopenmp-targets=amdgcn-amd-amdhsa
        -Xopenmp-target=amdgcn-amd-amdhsa -march=gfx906
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "-fopenmp")

elseif(BACKEND STREQUAL "NVC")
    if(ACC)
        set(_nvc_ompacc "-acc=gpu")
    else()
        set(_nvc_ompacc "-mp=gpu")
    endif()
    target_compile_options(umcx PRIVATE
        -Minfo=mp,accel -Minline
        -gpu=mem:managed,ptxinfo,ccall -static-nvidia ${_nvc_ompacc})
    set(_link_flags "-static-nvidia ${_nvc_ompacc}")

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
