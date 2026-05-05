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

option(ACC       "Use OpenACC instead of OpenMP for offload/CPU backends" OFF)
option(DEBUG     "Enable DEBUG preprocessor define"                       OFF)
option(BUILD_MEX "Build MATLAB MEX binding (output to umcxlab/)"          OFF)

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
if(ACC AND BACKEND STREQUAL "OMP")
    set(_ompacc "-fopenacc")
else()
    set(_ompacc "-fopenmp")
endif()

# ---------------------------------------------------------------------------
# Per-backend compile options and link flags — accumulated into variables so
# both the umcx executable and the optional MEX shared library can reuse them.
# ---------------------------------------------------------------------------
set(_backend_copts "")
set(_link_flags "")

if(BACKEND STREQUAL "SINGLE")
    # CPU single-thread, no extra flags

elseif(BACKEND STREQUAL "OMP")
    if(ACC)
        list(APPEND _backend_copts ${_ompacc} -Wno-unknown-pragmas)
        set(_link_flags "${_ompacc}")
    else()
        find_package(OpenMP REQUIRED)
        list(APPEND _backend_copts ${OpenMP_CXX_FLAGS})
        set(_link_flags "${OpenMP_CXX_FLAGS}")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            list(APPEND _backend_copts -foffload=disable)
            string(APPEND _link_flags " -foffload=disable")
        endif()
    endif()

elseif(BACKEND STREQUAL "NVIDIA")
    list(APPEND _backend_copts
        ${_ompacc} -foffload=nvptx-none -foffload=-lm
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "${_ompacc} -foffload=nvptx-none -no-pie")

elseif(BACKEND STREQUAL "NVIDIA_CLANG")
    list(APPEND _backend_copts
        ${_ompacc} -fopenmp-targets=nvptx64 --cuda-gpu-arch=${SM}
        -Wno-openmp-mapping -Wno-unused-command-line-argument)
    if(CUDA_PATH)
        list(APPEND _backend_copts --cuda-path=${CUDA_PATH})
    endif()
    set(_link_flags "${_ompacc}")

elseif(BACKEND STREQUAL "AMD")
    list(APPEND _backend_copts
        ${_ompacc} -foffload=amdgcn-amdhsa=-march=${GFX} -foffload=-lm
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "${_ompacc} -foffload=amdgcn-amdhsa=-march=${GFX} -no-pie")

elseif(BACKEND STREQUAL "AMD_CLANG")
    list(APPEND _backend_copts
        -target x86_64-pc-linux-gnu
        ${_ompacc} -fopenmp-targets=amdgcn-amd-amdhsa
        -Xopenmp-target=amdgcn-amd-amdhsa -march=${GFX}
        -fcf-protection=none -fno-stack-protector)
    set(_link_flags "${_ompacc}")

elseif(BACKEND STREQUAL "NVC")
    if(ACC)
        set(_nvc_gpu "-acc=gpu")
    else()
        set(_nvc_gpu "-mp=gpu")
    endif()
    list(APPEND _backend_copts
        -Minfo=mp,accel -Minline
        "-gpu=mem:managed,${CC_ARCH}" -static-nvidia ${_nvc_gpu})
    set(_link_flags "${_nvc_gpu} -gpu=mem:managed,${CC_ARCH} -static-nvidia")

else()
    message(FATAL_ERROR "Unknown BACKEND '${BACKEND}'. "
        "Valid values: OMP SINGLE NVIDIA NVIDIA_CLANG AMD AMD_CLANG NVC")
endif()

# Windows + GCC: statically link runtime to prevent DLL heap mismatch
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(APPEND _link_flags " -static-libgcc -static-libstdc++")
endif()

target_compile_options(umcx PRIVATE ${_backend_copts})
if(_link_flags)
    set_target_properties(umcx PROPERTIES LINK_FLAGS "${_link_flags}")
endif()

# ---------------------------------------------------------------------------
# MATLAB MEX binding  (cmake -DBUILD_MEX=ON [-DBACKEND=NVC|NVIDIA|...])
# Mirrors Makefile: make mex  /  make <target> MEX=1
# Output: umcxlab/umcx.<mexext>
#
# Uses add_library(MODULE) directly instead of matlab_add_mex() so that only
# libmex and libmx are linked — matching what "make mex" produces.
# ---------------------------------------------------------------------------
if(BUILD_MEX)
    find_package(Matlab COMPONENTS MX_LIBRARY)
    if(NOT Matlab_FOUND)
        message(WARNING "BUILD_MEX=ON but MATLAB not found; skipping MEX target")
    else()
        # Matlab_MEX_EXTENSION was added in CMake 3.14; fall back to platform default
        if(NOT DEFINED Matlab_MEX_EXTENSION)
            if(WIN32)
                set(Matlab_MEX_EXTENSION "mexw64")
            elseif(APPLE)
                if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
                    set(Matlab_MEX_EXTENSION "mexmaca64")
                else()
                    set(Matlab_MEX_EXTENSION "mexmaci64")
                endif()
            else()
                set(Matlab_MEX_EXTENSION "mexa64")
            endif()
        endif()

        # Use matlab_add_mex + NO_IMPLICIT_LINK_TO_MATLAB_LIBRARIES (CMake >= 3.24,
        # non-NVC) to link only libmex — matching "make mex" output.
        # Fall back to add_library(MODULE) for NVC (MATLAB's mex wrapper injects
        # GCC-specific CXXOPTIMFLAGS that nvc++ rejects) or older CMake where the
        # NO_IMPLICIT flag is unavailable.
        if(NOT BACKEND STREQUAL "NVC" AND NOT CMAKE_VERSION VERSION_LESS "3.24.0")
            matlab_add_mex(NAME umcxlab SRC umcx.cpp
                NO_IMPLICIT_LINK_TO_MATLAB_LIBRARIES
                LINK_TO ${Matlab_MEX_LIBRARY})
        else()
            add_library(umcxlab MODULE umcx.cpp)
            target_include_directories(umcxlab PRIVATE ${Matlab_INCLUDE_DIRS})
            target_link_libraries(umcxlab PRIVATE ${Matlab_MEX_LIBRARY})
        endif()
        target_include_directories(umcxlab PRIVATE .)
        target_compile_definitions(umcxlab PRIVATE MATLAB_MEX_FILE)
        if(BACKEND MATCHES "^(NVIDIA|NVIDIA_CLANG|AMD|AMD_CLANG|NVC)$")
            target_compile_definitions(umcxlab PRIVATE GPU_OFFLOAD)
        endif()
        target_compile_options(umcxlab PRIVATE -g -Wall -Wextra -pedantic -O3 ${_backend_copts})
        # SKIP_BUILD_RPATH: prevent CMake from embedding MATLAB's lib dir in RPATH;
        # without this, ldd would transitively resolve libmex.so's own dependencies
        # and show all of MATLAB's internal .so files, even though only libmex is
        # directly linked. MATLAB already has those libs loaded when it opens the MEX.
        set_target_properties(umcxlab PROPERTIES
            OUTPUT_NAME      "${CMAKE_SOURCE_DIR}/../umcxlab/umcx"
            PREFIX           ""
            SUFFIX           ".${Matlab_MEX_EXTENSION}"
            SKIP_BUILD_RPATH TRUE)
        if(_link_flags)
            set_target_properties(umcxlab PROPERTIES LINK_FLAGS "${_link_flags}")
        endif()
    endif()
endif()
