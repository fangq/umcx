# umcx - GPU-accelerated 3D Monte Carlo photon simulator in less than 1000 lines

* Copyright (C) 2024-2026  Qianqian Fang <q.fang at neu.edu>
* License: GNU General Public License version 3 (GPL v3), see LICENSE.txt
* Version: 0.5 （Boxer Crab)
* Website: https://mcx.space/umcx
* Github: https://github.com/fangq/umcx
* Acknowledgement: This project is part of the [MCX project](https://mcx.space)
  supported by US National Institute of Health (NIH)
  grant [R01-GM114365](https://reporter.nih.gov/search/sh5OXnkFp06HiLmhHXRj-Q/project-details/10701664#description)


[![uMCX CI](https://github.com/fangq/umcx/actions/workflows/run_test.yml/badge.svg)](https://github.com/fangq/umcx/actions/workflows/run_test.yml)

---

## Table of Contents

1. [Introduction](#introduction)
2. [Features](#features)
3. [Comparison with MCX](#comparison-with-mcx)
4. [How to compile umcx](#how-to-compile-umcx)
5. [MATLAB MEX binding (umcxlab)](#matlab-mex-binding-umcxlab)
6. [Hardware support status](#hardware-support-status)
7. [How to use umcx](#how-to-use-umcx)
8. [Command-line flags](#command-line-flags)
9. [Input file format](#input-file-format)
10. [Output file format](#output-file-format)
11. [Source types](#source-types)
12. [Built-in benchmarks](#built-in-benchmarks)
13. [How to run built-in tests](#how-to-run-built-in-tests)
14. [How to build documentation](#how-to-build-documentation)
15. [Acknowledgement](#acknowledgement)

---

## Introduction

**μMCX (umcx)** is a miniaturized, maximally portable Monte Carlo photon
transport simulator. It is designed to simulate light propagation in 3D
voxelated turbid media (such as biological tissues) with GPU acceleration
using the fewest possible lines of rule-formatted code.

umcx is designed around the following objectives:

- Must be shorter than 1000 lines after rule-based code formatting (**Portability, Adaptability**)
- Must support MCX's core functionality: 3D voxel-based Monte Carlo photon simulations with JSON inputs/outputs (**Compatibility, GPU-Acceleration**)
- Must support both CPU and GPU hardware across different vendors (**Portability**)
- Must be standard-compliant and compilable with diverse compilers (**Portability, Reusability**)
- Must be easily readable, easy to modify, and easy to adapt (**Readability, Adaptability**)

To meet these objectives, umcx is implemented with:

- **C++11**: Clean, object-oriented, portable standard C++
- **OpenMP 4.5 / OpenACC 2.0**: GPU offloading that works across NVIDIA, AMD, and Intel GPUs
- **JSON I/O**: Human-readable input/output using the [JSON for Modern C++](https://github.com/nlohmann/json) library and the [JData](https://neurojson.org/jdata) binary serialization format

umcx is backward-compatible with the [MCX](https://mcx.space) JSON input format,
allowing existing MCX simulations to run with minimal modification.

---

## Features

- 3D voxel-based photon Monte Carlo simulation
- Multi-region heterogeneous optical domains
- Refractive index mismatch and Fresnel reflection/refraction at boundaries
- Henyey-Greenstein anisotropic scattering phase function
- Multiple source types: pencil, isotropic, cone, disk, planar
- Time-gated simulation with configurable temporal windows
- Photon detection with partial path length recording (for DRS/DCS)
- Volumetric fluence-rate, fluence, or energy deposition output
- JSON and Binary JData (BJDATA/BNII) input/output compatible with MCX
- Built-in benchmark cases for validation
- Online simulation database access via [NeuroJSON.io](https://neurojson.io)
- GPU offloading via OpenMP 4.5 (`target`) and OpenACC 2.0 (`acc`)
- Single-source, single-file implementation (~840 lines)

---

## Comparison with MCX

umcx is a compact re-implementation of [MCX](https://mcx.space) that captures
its core Monte Carlo simulation functionality in roughly **24× fewer lines of
code**. The following tables compare MCX and umcx in terms of code size and
feature coverage.

### Code length

The table below maps each MCX source module to the corresponding class or
function in umcx. Line counts for umcx are measured after auto-formatting with
`astyle` (`make pretty`).

| MCX Lines | MCX File | uMCX Class / Function | uMCX Lines | Reduction |
|----------:|----------|----------------------|----------:|:---------:|
| 90 | `mcx.c` | `main()` | 8 | 11× |
| 940 | `mcx_shapes.c/.h` | `MCX_userio::initdomain()` | 43 | 21× |
| 313 | `mcx_tictoc.c/.h` | `MCX_clock` | 8 | 32× |
| 6036 | `mcx_utils.c/.h` | `MCX_userio` | 247 | 24× |
| 4875 | `mcx_core.cu/.h` | `MCX_run_simulation()` / `MCX_kernel()` / `MCX_photon` / `MCX_detect` | 53 / 56 / 236 / 37 | 13× |
| 140 | `mcx_rand_xorshift128p.cu` | `MCX_rand` | 29 | 5× |
| 156 | `mcx_const.h` | *(consolidated)* | — | — |
| 85 | `mcx_ieee754.h` | *(consolidated)* | — | — |
| 428 | `mcx_mie.cpp/.h` | *(not included)* | — | — |
| 7125 | `mcx_bench.c/.h` | `MCX_userio::benchmark()` | 36 | — |
| **20,188** | **total (core)** | **Total** | **843** | **24×** |
| 1374 | `mcxlab.cpp` | MATLAB/Octave binding | — | — |
| 1440 | `pmcx.cpp` | Python binding | — | — |
| **23,412** | **total (incl. bindings)** | — | — | — |

> The 843 umcx lines include 29 full-line comments and 69 blank lines.
> MCX's Mie scattering module (`mcx_mie.cpp/.h`) and language bindings
> (`mcxlab.cpp`, `pmcx.cpp`) have no equivalents in umcx.

### Feature comparison

Legend: **✔** = fully supported, **p** = partially supported (fraction indicates
covered / total variants), **t** = trivially implementable but omitted to
minimize code length, **—** = not implemented

| Feature | MCX | umcx |
|---------|:---:|:----:|
| Simulate any 3D label-based voxelated domain | ✔ | ✔ |
| Time-resolved simulation | ✔ | ✔ |
| Saving detected photon data (`-d`) | ✔ | ✔ |
| Boundary reflection (`-b`) | ✔ | ✔ |
| JSON input data file (`-f`/`-j`) | ✔ | ✔ |
| Shape-based media descriptor | ✔ | ✔ |
| NVIDIA GPU (`-G`) | ✔ | ✔ |
| Multi-GPU simulation | ✔ | — |
| CPU/GPU cross-vendor support | ✔ (mcxcl) | ✔ |
| Complex sources, focal length | ✔ | p (5/15) |
| Built-in benchmarks (`--bench`) | ✔ | p (8/10) |
| Customize detected-photon output (`-w`) | ✔ | p (4/8) |
| Widefield launch | ✔ | ✔ |
| JSON/Binary JSON data output | ✔ | ✔ |
| JSON data compression (`-z`) | ✔ | t |
| Patterned source | ✔ | — |
| Photon sharing | ✔ | — |
| Photon replay (`-q` / RF replay) | ✔ | — |
| Multi-source simulation | ✔ | — |
| Continuous medium formats (`-k`) | ✔ | — |
| Split-voxel MC (SVMC) | ✔ | — |
| Polarized light simulations | ✔ | — |
| User-defined launch distribution | ✔ | t |
| User-defined scattering phase function | ✔ | t |
| Boundary conditions (`-B`) | ✔ | — |
| MATLAB language binding | ✔ | ✔ |

---

## How to compile umcx

umcx is designed to be compatible with any C++ compiler that supports the C++11
standard and OpenMP/OpenACC GPU offloading. Supported compilers include:

| Compiler | Version | Notes |
|----------|---------|-------|
| `g++` (GCC) | ≥ 12 | CPU + NVIDIA/AMD GPU offloading |
| `nvc++` (NVIDIA HPC SDK) | any | Best NVIDIA GPU support via OpenACC/OpenMP |
| `clang++` (LLVM) | ≥ 16 | CPU + NVIDIA/AMD GPU offloading |
| `icpx` (Intel oneAPI) | any | CPU + Intel GPU via OpenMP |

All compilation is done from within the `src/` directory:

```bash
cd src
```

### Install compiler dependencies (Ubuntu/Debian)

**GCC 14 with OpenMP (CPU multi-threading only):**
```bash
sudo apt-get install g++-14
```

**GCC 14 with NVIDIA GPU offloading:**
```bash
sudo apt-get install g++-14 gcc-14-offload-nvptx
```

**GCC 14 with AMD GPU offloading:**
```bash
sudo apt-get install g++-14 gcc-14-offload-amdgcn
```

**LLVM/Clang 17 with NVIDIA GPU offloading:**
```bash
sudo apt-get install clang-17 libomp-17-dev
# LLVM OpenMP NVIDIA target libraries also required
```

**NVIDIA HPC SDK (nvc++):**

Download from [https://developer.nvidia.com/hpc-sdk](https://developer.nvidia.com/hpc-sdk)
and follow installation instructions. After installation:
```bash
export PATH=/opt/nvidia/hpc_sdk/Linux_x86_64/<version>/compilers/bin:$PATH
```

### Compilation targets

| Make target | Compiler | GPU support | Description |
|-------------|----------|-------------|-------------|
| `make` or `make all` | `g++` | None | Multi-core CPU via OpenMP (default) |
| `make multi` | `g++` | None | Same as `make all` |
| `make single` | `g++` | None | Single-core CPU (no threading) |
| `make omp` | `g++` | None | OpenMP CPU build |
| `make nvc` | `nvc++` | NVIDIA (OpenMP) | NVIDIA GPU via OpenMP target offload |
| `make nvc ACC=on` | `nvc++` | NVIDIA (OpenACC) | NVIDIA GPU via OpenACC |
| `make nvidia` | `g++` | NVIDIA | GCC nvptx offloading |
| `make nvidiaclang` | `clang++` | NVIDIA | Clang nvptx64 offloading |
| `make amd` | `g++` | AMD | GCC amdgcn offloading |
| `make amdclang` | `clang++` | AMD (gfx906) | Clang amdgcn offloading |
| `make debugsingle` | `g++` | None | Single-core debug build |
| `make debugmulti` | `g++` | None | Multi-core debug build |
| `make mex` | `g++` | None | MATLAB MEX file (CPU/OpenMP), output in `umcxlab/` |
| `make nvc MEX=1` | `nvc++` | NVIDIA | MATLAB MEX file (NVIDIA GPU via OpenMP) |
| `make nvidia MEX=1` | `g++` | NVIDIA | MATLAB MEX file (NVIDIA GPU, GCC nvptx) |
| `make nvidiaclang MEX=1` | `clang++` | NVIDIA | MATLAB MEX file (NVIDIA GPU, Clang) |
| `make amdclang MEX=1` | `clang++` | AMD | MATLAB MEX file (AMD GPU) |
| `make doc` | doxygen | — | Generate HTML/LaTeX documentation |
| `make clean` | — | — | Remove binary, objects, doc output, and `umcxlab/umcx.mex*` |
| `make pretty` | astyle | — | Auto-format source code |

The compiled binary is placed in `../bin/umcx`.

**Example: compile for CPU multi-core (OpenMP):**
```bash
cd src
make
```

**Example: compile for NVIDIA GPU with nvc++:**
```bash
cd src
make nvc
```

**Example: compile for AMD GPU with GCC:**
```bash
cd src
make amd
```

### CMake-based compilation

CMake (≥ 3.5) is supported as an alternative build system. The `CMakeLists.txt`
is located in `src/` alongside the Makefile.

```bash
cd src
cmake -B ../build          # configure (default: OMP backend)
cmake --build ../build     # compile
```

The binary is placed in `../bin/umcx`, matching the Makefile output location.

**CMake options:**

| Option | Default | Description |
|--------|---------|-------------|
| `BACKEND` | `OMP` | Backend: `OMP` `SINGLE` `NVIDIA` `NVIDIA_CLANG` `AMD` `AMD_CLANG` `NVC` |
| `ACC` | `OFF` | Use OpenACC instead of OpenMP for the `NVC` backend |
| `DEBUG` | `OFF` | Enable `DEBUG` preprocessor define |
| `BUILD_MEX` | `OFF` | Build MATLAB MEX binding; output to `umcxlab/umcx.<mexext>` |
| `CUDA_PATH` | *(empty)* | CUDA installation path for `NVIDIA_CLANG` backend |
| `CC_ARCH` | `cc70,cc80,cc86,cc90,ptx` | nvc++ GPU targets; `ptx` embeds PTX for JIT-based forward compatibility with future GPUs |

**Examples:**

```bash
# CPU multi-core (default, equivalent to make multi)
cmake -B ../build -DBACKEND=OMP && cmake --build ../build

# NVIDIA GPU via nvc++ with OpenMP offload (equivalent to make nvc)
cmake -B ../build -DCMAKE_CXX_COMPILER=nvc++ -DBACKEND=NVC
cmake --build ../build

# NVIDIA GPU via nvc++ with OpenACC (equivalent to make nvc ACC=on)
cmake -B ../build -DCMAKE_CXX_COMPILER=nvc++ -DBACKEND=NVC -DACC=ON
cmake --build ../build

# NVIDIA GPU via GCC offload (equivalent to make nvidia)
cmake -B ../build -DBACKEND=NVIDIA && cmake --build ../build

# NVIDIA GPU via Clang (equivalent to make nvidiaclang)
cmake -B ../build -DCMAKE_CXX_COMPILER=clang++ -DBACKEND=NVIDIA_CLANG \
      -DCUDA_PATH=/usr/local/cuda
cmake --build ../build

# AMD GPU via GCC offload (equivalent to make amd)
cmake -B ../build -DBACKEND=AMD && cmake --build ../build

# AMD GPU via Clang (equivalent to make amdclang)
cmake -B ../build -DCMAKE_CXX_COMPILER=clang++ -DBACKEND=AMD_CLANG
cmake --build ../build

# MATLAB MEX file, CPU/OpenMP (equivalent to make mex)
cmake -B ../build -DBUILD_MEX=ON && cmake --build ../build --target umcxlab

# MATLAB MEX file, NVIDIA GPU via nvc++ (equivalent to make nvc MEX=1)
cmake -B ../build -DCMAKE_CXX_COMPILER=nvc++ -DBACKEND=NVC -DBUILD_MEX=ON
cmake --build ../build --target umcxlab
```

### Code formatting

Because code length is a core specification of umcx, the canonical line count
is measured only after auto-formatting with `astyle`. Before each commit or
line-count measurement, run:

```bash
make pretty
```

This requires `astyle` to be installed:
```bash
sudo apt-get install astyle
```

---

## MATLAB MEX binding (umcxlab)

umcx includes a MATLAB MEX binding that exposes the simulation engine as a
native MATLAB function. The binding lives in `umcxlab/` and is largely
compatible with [MCXLab](https://mcx.space/wiki/index.cgi?Doc/mcxlab), but
limited to the options supported by umcx.

### Building the MEX file

MATLAB must be installed and `mex` must be on `PATH` (or set `MEX_BIN`).

**CPU / OpenMP (recommended for most users):**
```bash
cd src
make mex
```

**NVIDIA GPU (nvc++, best performance):**
```bash
make nvc MEX=1
```

**NVIDIA GPU (GCC nvptx or Clang):**
```bash
make nvidia MEX=1        # GCC nvptx offloading
make nvidiaclang MEX=1   # Clang nvptx64 offloading
```

**AMD GPU (ROCm clang++):**
```bash
make amdclang MEX=1
```

The `MEX=1` flag can be appended to any GPU build target. The compiled file
is placed in `umcxlab/umcx.mexa64` (Linux), `umcx.mexmaci64` (macOS), or
`umcx.mexw64` (Windows). `make clean` also removes `umcxlab/umcx.mex*`.

### Usage

Add `umcxlab/` to your MATLAB path and call `umcxlab` with an MCX-compatible
configuration struct:

```matlab
addpath('/path/to/umcx/umcxlab');

cfg.nphoton  = 1e6;
cfg.vol      = ones(60, 60, 60, 'uint8');
cfg.srcpos   = [30 30 1];
cfg.srcdir   = [0 0 1];
cfg.prop     = [0 0 1 1; 0.005 1 0.01 1.37];
cfg.tstart   = 0;
cfg.tend     = 5e-9;
cfg.tstep    = 5e-9;

[flux, detp] = umcxlab(cfg);
```

Or using a built-in benchmark via `mcxcreate` (from MCXLab):
```matlab
[flux, detp] = umcxlab(mcxcreate('cube60'));
```

`umcxlab` serializes `cfg` to BJData format via `mcx2json`, passes the binary
blob to the `umcx` MEX entry point, and returns:

- `flux.data` — 3D or 4D `single` array of fluence-rate (or fluence/energy,
  per `cfg.outputtype`), shape `[Nx, Ny, Nz, Nt]`
- `detp.data` — 2D `single` array of detected-photon records,
  shape `[ndetected × ncolumns]`

### Compatibility with MCXLab

`umcxlab` accepts the same configuration struct format as `mcxlab`. Fields not
supported by umcx are silently ignored by `mcx2json`. The table below
summarizes the main differences:

| Feature | mcxlab | umcxlab |
|---------|:------:|:-------:|
| GPU acceleration | ✔ | ✔ (compile-time choice) |
| Multiple output types (`outputtype`) | ✔ | ✔ |
| Boundary reflection (`DoMismatch`) | ✔ | ✔ |
| Detected photon output (`detp`) | ✔ | ✔ |
| Source types | 15 | 5 (pencil/isotropic/cone/disk/planar) |
| Multi-GPU | ✔ | — |
| Photon replay | ✔ | — |
| Polarized light | ✔ | — |
| Continuous medium (`mediabyte`) | ✔ | — |
| Python binding (pmcx) | ✔ | — |

---

## Hardware support status

The table below summarizes the current hardware support status for each
compilation target. Status is tested on Linux x86-64.

| Make target | Compiler | Hardware | Status | Notes |
|-------------|----------|----------|:------:|-------|
| `make` / `make multi` | `g++` ≥ 12 | CPU (multi-core) | ✔ Works | Standard OpenMP threading; default build |
| `make single` | `g++` ≥ 12 | CPU (single-core) | ✔ Works | No threading; useful for debugging |
| `make nvc` | `nvc++` | NVIDIA GPU (OpenMP) | ✔ Works | Best NVIDIA performance via `libcuda.so` |
| `make nvc ACC=on` | `nvc++` | NVIDIA GPU (OpenACC) | ✔ Works | OpenACC path; similar performance to `nvc` |
| `make nvidia` | `g++` ≥ 12 | NVIDIA GPU | ✔ Works | GCC nvptx offloading; falls back to CPU if no GPU |
| `make nvidiaclang` | `clang++` ≥ 16 | NVIDIA GPU | ✔ Works | Clang nvptx64 offloading; requires `--cuda-path` |
| `make amdclang` | ROCm `clang++` ≥ 17 | AMD GPU | ✔ Works | Requires ROCm ≥ 6.1; specify `GFX=<arch>` |
| `make amd` | `g++` ≥ 12 | AMD GPU | ✘ Broken | GCC 13 `libgomp-plugin-amdgcn` runtime bug (see below) |

### NVIDIA GPU

- **`make nvc`** (NVIDIA HPC SDK `nvc++`): Full NVIDIA GPU support via
  OpenMP `target` or OpenACC `kernels`. This binary **requires** the CUDA
  driver (`libcuda.so`) to be present at runtime even with `-static-nvidia`
  (which only statically links `libcudart`, not the driver API). There is no
  automatic CPU fallback if the CUDA driver is absent — the process will
  abort with a library-not-found error.

  The default `CC_ARCH=cc70,cc80,cc86,cc90,ptx` embeds native CUBIN for
  common Turing–Hopper GPUs plus PTX as a JIT fallback for any GPU not
  explicitly listed. The CUDA driver JIT-compiles the PTX at first run (result
  is cached), so the same binary runs on future architectures without
  recompilation. PTX forward compatibility is bounded by the HPC SDK version
  used to build: nvc++ 24.11 supports up to sm_90; for RTX 5090 (cc120 /
  Blackwell) use HPC SDK 25.1 or later. Override with e.g.
  `make nvc CC_ARCH=cc90,cc100,cc120,ptx` to add explicit Blackwell support.

- **`make nvidia`** (GCC nvptx) and **`make nvidiaclang`** (Clang nvptx64):
  These embed PTX (NVIDIA's virtual ISA) in the binary. The CUDA driver JIT-
  compiles the PTX for the actual GPU at runtime, so a binary compiled with
  `SM=sm_50` will run correctly on newer GPU generations (sm_70, sm_86,
  sm_90, …) — forward compatibility is preserved. If no GPU is detected,
  libgomp falls back to executing the target region on the CPU.

- **Architecture selection** (`SM`): Override with e.g. `make nvidia SM=sm_86`
  or `cmake -DSM=sm_86`. The default `sm_50` (Maxwell) covers all GPUs since
  2014; note that CUDA 12.8+ dropped ptxas support for sm_50/sm_60, so use
  `SM=sm_70` or higher when building with a recent CUDA 12 toolkit.

### AMD GPU

- **`make amdclang`** (ROCm `clang++` ≥ 17, roc-6.1.1 tested): Full AMD GPU
  support via OpenMP `target` offloading. The default compiler path is
  `/opt/rocm/llvm/bin/clang++`; override with `make amdclang AMDCXX=/path/to/clang++`.

- **`make amd`** (GCC ≥ 12 `libgomp-plugin-amdgcn`): **Currently broken** —
  even a trivial GPU kernel crashes at runtime with a `Memory access fault /
  Page not present` error. The root cause is a bug in GCC 13's
  `libgomp-plugin-amdgcn1` where the per-team state buffer pointer is
  uninitialized. Use `make amdclang` instead.

- **No forward compatibility**: Unlike NVIDIA's PTX, AMD GCN ISA is tied to a
  specific GPU generation. A binary compiled for `gfx906` (Radeon VII /
  Vega 20) will **not** run on `gfx1010` (RDNA 1) or newer architectures.
  Always specify the correct architecture: `make amdclang GFX=gfx1100` for
  RDNA 3 (RX 7000 series), `GFX=gfx90a` for MI200, etc. Run
  `rocminfo | grep gfx` to find your GPU's architecture string.

- **Architecture selection** (`GFX`): Override with e.g.
  `make amdclang GFX=gfx1030` or `cmake -DGFX=gfx1030`. Default is `gfx906`.

### Portability summary

Unlike OpenCL (which compiles to a portable IR and JITs at runtime for any
supported GPU), OpenMP/OpenACC offloading compiles AOT (Ahead-Of-Time) to a
specific ISA. A single umcx binary can only target **one GPU architecture per
vendor** unless you specify multiple `-foffload` targets at compile time (GCC
supports fat binaries with multiple `-foffload=` flags).

| Scenario | Behavior |
|----------|----------|
| Run `nvc`-built binary without NVIDIA GPU/driver | **Aborts** — CUDA driver required |
| Run `nvc`-built binary on a GPU newer than `CC_ARCH` | Works — PTX fallback JIT-compiled by CUDA driver (requires HPC SDK new enough to know the GPU's PTX ISA) |
| Run `nvidia`/`nvidiaclang`-built binary without GPU | Falls back to CPU |
| Run `nvidia`-built `SM=sm_50` binary on newer NVIDIA GPU | Works — PTX is JIT-compiled |
| Run `amdclang`-built `GFX=gfx906` binary on a different AMD GPU | **Fails** — wrong ISA |
| Run CPU (`make`) binary on any x86-64 machine | Works — no GPU needed |

> **Standards note:** umcx uses OpenMP 4.5 for GPU offloading (`target teams distribute parallel for`,
> `reduction` on combined target constructs) and OpenACC 2.0 (`firstprivate`, `atomic capture`).
> The struct-plus-pointer-member mapping pattern (`map(to: s, s.ptr[0:N])`) relies on pointer
> attachment behavior that all modern compilers implement correctly for OpenMP 4.5, though the
> formal spec guarantee was added in OpenMP 5.0.

---

## How to use umcx

The compiled binary is `bin/umcx` (or on PATH as `umcx`). It accepts input in
three equivalent forms:

### 1. Run with a JSON input file
```bash
umcx myinput.json
```

### 2. Run a built-in benchmark
```bash
umcx cube60
umcx -Q skinvessel
```

### 3. Run with command-line flags
```bash
umcx -Q cube60 -n 1e7 -s myresult -U 1
```

### 4. Override JSON settings with an inline JSON string
```bash
umcx myinput.json -j '{"Session":{"Photons":5000000}}'
umcx -Q cube60 --json '{"Optode":{"Source":{"Type":"isotropic","Pos":[29,29,29]}}}'
```

### 5. Browse and download from NeuroJSON.io online database
```bash
# List all available MCX simulations
umcx -N

# Download and run a specific simulation from NeuroJSON.io
umcx -N colin27
```
> Requires `curl` to be installed.

### 6. Inspect simulation settings without running
```bash
# Print full JSON configuration (useful for debugging or sharing settings)
umcx -Q cube60 --dumpjson

# Export the volumetric domain mask as a binary JSON file
umcx -Q cube60 --dumpmask
```

### Typical workflow

```bash
# 1. Inspect what a built-in benchmark looks like as JSON
./bin/umcx --bench cube60 --dumpjson > mycube.json

# 2. Edit mycube.json to customize geometry, media, source

# 3. Run the simulation
./bin/umcx mycube.json -n 1e7

# 4. Outputs are saved to <SessionID>.bnii and <SessionID>_detp.jdb
```

---

## Command-line flags

```
umcx [options] [inputfile.json | benchmarkname]
```

| Short | Long | Default | Description |
|-------|------|---------|-------------|
| `-f`  | `--input` | — | Load configuration from a JSON file |
| `-Q`  | `--bench` | — | Run a built-in benchmark by name |
| `-n`  | `--photon` | `1e6` | Number of photons to simulate |
| `-s`  | `--session` | — | Output session name (prefix for output files) |
| `-u`  | `--unitinmm` | `1` | Voxel edge length in millimeters |
| `-E`  | `--seed` | `1648335518` | Random number generator seed |
| `-O`  | `--outputtype` | `x` | Output type: `x`=fluence-rate, `f`=fluence, `e`=energy |
| `-b`  | `--reflect` | `0` | Enable refractive-index-mismatch boundary handling (`1`=on) |
| `-d`  | `--savedet` | `1` | Save detected photon data (`1`=on, `0`=off) |
| `-w`  | `--savedetflag` | `5` | Detected photon data fields (bit flags, see table below) |
| `-H`  | `--maxdetphoton` | `1000000` | Maximum number of detected photons to store |
| `-S`  | `--save2pt` | `1` | Save volumetric output (`1`=on, `0`=off) |
| `-U`  | `--normalize` | `1` | Normalize output (`1`=on, `0`=off) |
| `-t`  | `--thread` | auto | Total number of threads (GPU: total work-items) |
| `-T`  | `--blocksize` | `64` | Thread block/team size (GPU: work-group size) |
| `-G`  | `--gpuid` | `1` | GPU device ID |
| `-j`  | `--json` | — | JSON string to merge/overwrite current settings |
| `-h`  | `--help` | — | Print help message and list benchmarks |
| `-N`  | `--net` | — | Browse or download simulations from NeuroJSON.io |
|       | `--dumpjson` | — | Print full JSON configuration and exit (no simulation) |
|       | `--dumpmask` | — | Save volumetric domain mask to binary JSON and exit |

### `--savedetflag` bit values

The `-w`/`--savedetflag` option is a bitmask controlling which fields are stored
for each detected photon. Add together the bits for the desired fields:

| Bit value | Field | Description |
|-----------|-------|-------------|
| `1` | Detector ID | Index of the detector that captured the photon |
| `4` | Partial path | Path length (mm) traversed in each medium |
| `16` | Exit position | [x, y, z] coordinates where photon exits the domain |
| `32` | Exit direction | [vx, vy, vz] unit vector of photon direction at exit |

Default (`-w 5`) saves detector ID + partial path lengths. To save all fields:
```bash
umcx -Q cube60b -w 53   # 1 + 4 + 16 + 32
```

---

## Input file format

umcx uses JSON as its primary input format, compatible with the
[MCX JSON input specification](https://mcx.space/wiki/index.cgi?Doc/mcx_help).
The input file contains five top-level sections:

### Top-level structure

```json
{
  "Session":  { ... },
  "Forward":  { ... },
  "Domain":   { ... },
  "Optode":   { ... },
  "Shapes":   [ ... ]
}
```

---

### `Session` — simulation control

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `ID` | string | `""` | Output file name prefix |
| `Photons` | int | `1000000` | Number of photons to simulate |
| `RNGSeed` | int | `1648335518` | Random number generator seed |
| `DoMismatch` | bool | `false` | Enable Fresnel reflection/refraction at boundaries |
| `DoSaveVolume` | bool | `true` | Save volumetric fluence output |
| `DoNormalize` | bool | `true` | Normalize volumetric output |
| `DoPartialPath` | bool | `true` | Save detected photon partial path data |
| `DoSaveRef` | bool | `false` | Save boundary reflection data |
| `DoSaveExit` | bool | `false` | Save exit position/direction of detected photons |
| `DoSaveSeed` | bool | `false` | Save RNG seeds for photon replay |
| `DoAutoThread` | bool | `true` | Automatically determine thread count |
| `DoDCS` | bool | `false` | Enable diffuse correlation spectroscopy output |
| `DoSpecular` | bool | `false` | Include specular reflection at source entry |
| `DebugFlag` | int | `0` | Debug verbosity level |
| `OutputFormat` | string | `"jnii"` | Output file format (`"jnii"` = binary JData NIFTI) |
| `OutputType` | string | `"x"` | Output quantity: `"x"` fluence-rate, `"f"` fluence, `"e"` energy |
| `MaxDetPhoton` | int | `1000000` | Maximum detected photon buffer size |
| `SaveDetFlag` | int | `5` | Detected photon data fields (same as `-w`, see above) |
| `ThreadNum` | int | auto | Total number of GPU work-items |
| `BlockSize` | int | `64` | GPU work-group (thread block) size |
| `DeviceID` | int | `1` | GPU device index |

---

### `Forward` — temporal integration

| Key | Type | Unit | Description |
|-----|------|------|-------------|
| `T0` | float | seconds | Simulation start time |
| `T1` | float | seconds | Simulation end time |
| `Dt` | float | seconds | Time-gate width (bin size) |

Example: a single 5 ns time gate:
```json
"Forward": { "T0": 0, "T1": 5e-9, "Dt": 5e-9 }
```

---

### `Domain` — optical medium and volume

| Key | Type | Description |
|-----|------|-------------|
| `Dim` | int[3] | Domain dimensions [Nx, Ny, Nz] in voxels |
| `LengthUnit` | float | Voxel edge length in millimeters (default `1`) |
| `OriginType` | int | `0`=corner origin, `1`=grid-aligned origin |
| `MediaFormat` | string | Voxel data type: `"byte"` (uint8) or `"integer"` (uint32) |
| `Media` | array | List of medium optical properties (index 0 = background/void) |

Each entry in `Media` is:

| Key | Type | Unit | Description |
|-----|------|------|-------------|
| `mua` | float | mm⁻¹ | Absorption coefficient |
| `mus` | float | mm⁻¹ | Scattering coefficient |
| `g` | float | — | Henyey-Greenstein anisotropy factor (0–1) |
| `n` | float | — | Refractive index |

> Index 0 is always the background medium (typically void/air: `mua=0, mus=0, g=1, n=1`).
> Voxels with value `k` in the domain volume use `Media[k]`.

**Example media definition:**
```json
"Domain": {
  "Dim": [60, 60, 60],
  "LengthUnit": 1,
  "Media": [
    {"mua": 0.00, "mus": 0.0,  "g": 1.00, "n": 1.00},
    {"mua": 0.02, "mus": 9.0,  "g": 0.89, "n": 1.37},
    {"mua": 0.04, "mus": 0.01, "g": 0.89, "n": 1.37}
  ]
}
```

---

### `Optode` — light source and detectors

#### Source (`/Optode/Source`)

| Key | Type | Description |
|-----|------|-------------|
| `Type` | string | Source type (see [Source types](#source-types)) |
| `Pos` | float[3] | Source position [x, y, z] in voxels |
| `Dir` | float[3/4] | Propagation direction unit vector [vx, vy, vz] (optional 4th element w is unused) |
| `Param1` | float[4] | Source-type-specific parameter 1 (see source type table) |
| `Param2` | float[4] | Source-type-specific parameter 2 (see source type table) |
| `SrcNum` | int | Number of simultaneous sources (default `1`) |

#### Detectors (`/Optode/Detector`)

An array of circular detector objects:

| Key | Type | Description |
|-----|------|-------------|
| `Pos` | float[3] | Detector center position [x, y, z] in voxels |
| `R` | float | Detector radius in voxels |

**Example:**
```json
"Optode": {
  "Source": {
    "Type": "pencil",
    "Pos": [30, 30, 0],
    "Dir": [0, 0, 1]
  },
  "Detector": [
    {"Pos": [30, 40, 0], "R": 1.5},
    {"Pos": [30, 50, 0], "R": 1.5}
  ]
}
```

---

### `Shapes` — volumetric domain construction

The `Shapes` array defines geometric primitives that are rasterized (painted) into
the 3D domain volume in order. Each shape object tags voxels inside it with a
medium index. Shapes are applied sequentially; later shapes overwrite earlier ones.

Alternatively, `Shapes` can contain a pre-computed volume array in JData format:

```json
"Shapes": [
  {"_ArrayType_": "uint8", "_ArraySize_": [Nx, Ny, Nz], "_ArrayData_": [...]}
]
```

#### Supported shape primitives

| Shape key | Required fields | Description |
|-----------|----------------|-------------|
| `Grid` | `Tag`, `Size[3]` | Fill the entire grid with a medium |
| `Sphere` | `O[3]`, `R`, `Tag` | Sphere with center `O` and radius `R` |
| `Box` | `O[3]`, `Size[3]`, `Tag` | Axis-aligned box with corner `O` and size `Size` |
| `Cylinder` | `C0[3]`, `C1[3]`, `R`, `Tag` | Cylinder between endpoints `C0` and `C1` with radius `R` |
| `XLayers` | array of `[xmin, xmax, tag]` | Slabs perpendicular to X axis |
| `YLayers` | array of `[ymin, ymax, tag]` | Slabs perpendicular to Y axis |
| `ZLayers` | array of `[zmin, zmax, tag]` | Slabs perpendicular to Z axis |

All coordinates are in voxel units.

**Example shapes definition:**
```json
"Shapes": [
  {"Grid":   {"Tag": 1, "Size": [60, 60, 60]}},
  {"Sphere": {"O": [30, 30, 30], "R": 15, "Tag": 2}}
]
```

---

### Complete example input (colin27.json excerpt)

```json
{
  "Session": {
    "ID": "colin27",
    "Photons": 1000000,
    "RNGSeed": 1648335518,
    "DoMismatch": true,
    "DoSaveVolume": true,
    "DoNormalize": true,
    "DoPartialPath": true,
    "OutputFormat": "jnii",
    "OutputType": "x"
  },
  "Forward": {"T0": 0, "T1": 5e-9, "Dt": 5e-9},
  "Domain": {
    "MediaFormat": "byte",
    "LengthUnit": 1,
    "Dim": [181, 217, 181],
    "Media": [
      {"mua": 0,     "mus": 0,      "g": 1,    "n": 1    },
      {"mua": 0.019, "mus": 7.8182, "g": 0.89, "n": 1.37 },
      {"mua": 0.019, "mus": 7.8182, "g": 0.89, "n": 1.37 },
      {"mua": 0.0004,"mus": 0.009,  "g": 0.89, "n": 1.37 },
      {"mua": 0.02,  "mus": 9,      "g": 0.89, "n": 1.37 },
      {"mua": 0.08,  "mus": 40.9,   "g": 0.89, "n": 1.37 }
    ]
  },
  "Optode": {
    "Source": {
      "Type": "pencil",
      "Pos": [75, 67.38, 167.5],
      "Dir": [0.1636, 0.4569, -0.8743, 0]
    },
    "Detector": [
      {"Pos": [75, 77.19, 170.3], "R": 1},
      {"Pos": [75, 89.0,  170.3], "R": 1}
    ]
  }
}
```

---

## Output file format

umcx produces two output files per simulation in
[Binary JData (BJDATA)](https://neurojson.org/bjdata) format, which is a
binary encoding of JSON and is fully readable/writable with the
[JData toolbox](https://neurojson.org) in MATLAB, Python, and other languages.

### Volumetric output (`<SessionID>.bnii`)

Saved when `Session/DoSaveVolume` is `true` (default). The file is a NIFTI-formatted
Binary JData file containing the 3D or 4D volumetric result.

```
File: <SessionID>.bnii
```

Top-level structure:
```json
{
  "NIFTIHeader": {
    "Dim": [Nx, Ny, Nz, Nt]
  },
  "NIFTIData": {
    "_ArrayType_": "single",
    "_ArraySize_": [Nx, Ny, Nz, Nt],
    "_ArrayOrder_": "c",
    "_ArrayData_": [...]
  }
}
```

The meaning of voxel values depends on `Session/OutputType`:

| `OutputType` | Description | Normalization factor |
|-------------|-------------|---------------------|
| `"x"` | Fluence rate (mm⁻² s⁻¹) | `Dt / (nphoton × unitinmm²)` |
| `"f"` | Fluence (mm⁻²) | `1 / (nphoton × unitinmm²)` |
| `"e"` | Energy deposition (a.u.) | `1 / nphoton` |

The 4th dimension `Nt = (T1 - T0) / Dt` is the number of time gates.
With a single time gate (`T0=0`, `T1=Dt`), `Nt=1` and the output is 3D.

---

### Detected photon output (`<SessionID>_detp.jdb`)

Saved when `Session/DoPartialPath` is `true` (default). Contains one record
per detected photon.

```
File: <SessionID>_detp.jdb
```

Top-level structure:
```json
{
  "MCXData": {
    "Info": {
      "Version": 1,
      "MediaNum": <number of media>,
      "DetNum": <number of detectors>,
      "ColumnNum": <floats per photon record>,
      "TotalPhoton": <photons launched>,
      "DetectedPhoton": <photons that reached a detector>,
      "SavedPhoton": <photons saved to file>,
      "LengthUnit": <voxel size in mm>
    },
    "PhotonRawData": {
      "_ArrayType_": "single",
      "_ArraySize_": [SavedPhoton, ColumnNum],
      "_ArrayData_": [...]
    }
  }
}
```

Each row of `PhotonRawData` contains the following fields (in order),
depending on the `SaveDetFlag` bitmask:

| Bit | Field | Columns | Description |
|-----|-------|---------|-------------|
| `1` | Detector ID | 1 | 1-based index of the detector that captured the photon |
| `4` | Partial path | `MediaNum` | Path length (mm) in each medium (index matches `Domain/Media`) |
| `16` | Exit position | 3 | [x, y, z] coordinates (voxels) at domain boundary exit |
| `32` | Exit direction | 3 | [vx, vy, vz] unit vector at domain boundary exit |

Default `SaveDetFlag=5` (bits 1+4) yields `1 + MediaNum` columns per photon.

**Reading detected photon data in MATLAB** (using the
[JData toolbox](https://github.com/NeuroJSON/jsnirfy)):
```matlab
data = loadjson('mysim_detp.jdb');
ppath = data.MCXData.PhotonRawData(:, 2:end);  % partial paths, shape [ndet x nmedia]
detid = data.MCXData.PhotonRawData(:, 1);      % detector IDs
```

---

## Source types

The source type is set via `Optode/Source/Type`. The following types are supported:

| Type | Description | `Param1` | `Param2` |
|------|-------------|----------|----------|
| `pencil` | Collimated point beam; all photons launch in direction `Dir` | — | — |
| `isotropic` | Point source; photons emitted uniformly in all directions | — | — |
| `cone` | Cone beam; photons uniformly distributed within a cone around `Dir` | `[half_angle, 0, 0, 0]` (radians) | — |
| `disk` | Disk (top-hat) source; photons uniformly distributed over a disk centered at `Pos` in the plane perpendicular to `Dir` | `[outer_radius, inner_radius, 0, 0]` (mm) | — |
| `planar` | Planar (rectangular) source; photons uniformly distributed over a parallelogram | `[edge1_x, edge1_y, edge1_z, 0]` | `[edge2_x, edge2_y, edge2_z, 0]` |

**Example: disk source with 5 mm radius:**
```json
"Source": {
  "Type": "disk",
  "Pos": [30, 30, 0],
  "Dir": [0, 0, 1],
  "Param1": [5, 0, 0, 0]
}
```

**Example: planar widefield source (10×10 mm patch):**
```json
"Source": {
  "Type": "planar",
  "Pos": [20, 20, 0],
  "Dir": [0, 0, 1],
  "Param1": [10, 0, 0, 0],
  "Param2": [0, 10, 0, 0]
}
```

---

## Built-in benchmarks

umcx includes seven built-in benchmark cases. They can be run with:

```bash
umcx <benchmarkname>
umcx -Q <benchmarkname>
umcx -Q <benchmarkname> -n 1e7    # override photon count
```

| Name | Domain size | Source | Media | Notes |
|------|------------|--------|-------|-------|
| `cube60` | 60³ voxels | Pencil at (29,29,0) | 3 (homogeneous) | No reflection |
| `cube60b` | 60³ voxels | Pencil at (29,29,0) | 3 (homogeneous) | With boundary reflection |
| `cube60planar` | 60³ voxels | Planar 40×40 mm | 3 (homogeneous) | Widefield illumination |
| `cubesph60b` | 60³ voxels | Pencil | 3 | Sphere (r=15) embedded in cube |
| `sphshells` | 60³ voxels | Pencil | 4 | Three concentric spherical shells |
| `spherebox` | 60³ voxels | Pencil | 3 | Sphere (r=10) with short time gate |
| `skinvessel` | 200³ voxels | Disk | 5 | Realistic skin + cylindrical vessel (r=10); `LengthUnit=0.005` mm |

All benchmarks use `T0=0`, `T1=5e-9 s`, `Dt=5e-9 s` by default (single 5 ns gate).

**Expected console output** (example for `cube60`):
```
simulated energy 1000000, speed 3245 photon/ms, duration 308 ms,
normalizer 5e-09, detected 412, absorbed 17.3%
```

To print the full JSON configuration of a benchmark without running:
```bash
umcx --bench cube60 --dumpjson
```

---

## How to run built-in tests

A shell-based test suite is located in `test/testumcx.sh`. It runs a series of
functional tests verifying benchmark outputs, flag behavior, and boundary conditions.

```bash
cd test
bash testumcx.sh
```

The script automatically finds the `umcx` binary in `../bin/umcx` or on `$PATH`.
On success:
```
passed all tests!
```

On failure, the script prints the failing test name and exits with a non-zero code.

The test suite covers:
- Binary existence and executable permissions
- Shared library linkage
- Help text output
- Built-in benchmark listing
- JSON export (`--dumpjson`)
- JSON override (`--json`)
- Homogeneous domain simulation (cube60)
- Boundary reflection (cube60b, `-b 1`)
- Photon detection (cube60b)
- Planar widefield source (cube60planar)
- Isotropic and cone beam sources
- Heterogeneous domain (spherebox)
- Skin vessel with `unitinmm` scaling (skinvessel)
- Memory safety (valgrind, if installed)

---

## How to build documentation

umcx uses [Doxygen](https://www.doxygen.nl) for API documentation. The Doxygen
configuration is in `src/umcxdoc.cfg`.

Install Doxygen (Ubuntu/Debian):
```bash
sudo apt-get install doxygen
```

Build documentation:
```bash
cd src
make doc
```

The generated HTML documentation is placed in `doc/`. Open `doc/index.html` in
a browser to browse the API documentation.

---

## License

umcx is released under the **GNU General Public License version 3 (GPL v3)**.
See `LICENSE.txt` for the full license text.

---

## Citation

If you use umcx in a publication, please cite the MCX project:

> Qianqian Fang, "μMCX - Modern, Easy-to-Adapt, Hardware-Accelerated 3D Monte Carlo
> Photon Simulator in 800 Lines of Code," Optica Biophotonics Congress 2026, Paper OS3D.3

---

## Acknowledgement

The authors would like to thank Mat Colgrove at NVIDIA for suggestions on
OpenMP offloading and code optimization.

This project is supported by the US National Institutes of Health (NIH)
under grant [R01-GM114365](https://reporter.nih.gov/search/sh5OXnkFp06HiLmhHXRj-Q/project-details/10701664#description).

---

### Embedded third-party components

umcx bundles the following open-source libraries directly in the source tree
(no separate installation required):

| Component | Version | Location | License | Description |
|-----------|---------|----------|---------|-------------|
| [JSON for Modern C++](https://github.com/nlohmann/json) | 3.11.3 | `src/nlohmann/json.hpp` | MIT | Single-header C++11 JSON parser and serializer by Niels Lohmann |
| [ZMat](https://github.com/NeuroJSON/zmat) | — | `src/zmat` | GPLv3 | Single-header zlib/deflate compression library used for binary JData output |
| [miniz](https://github.com/richgel999/miniz) | — | `src/zmat` | MIT | Single-file zlib/deflate compression library, embedded inside zmat.h |
