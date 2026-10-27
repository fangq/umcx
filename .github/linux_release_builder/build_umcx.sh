#!/bin/bash
# Builds the portable Linux release binaries of umcx inside the manylinux_2_28 builder:
#
#   docker build --build-arg WITH_ROCM=1 -t umcx-linux-release-builder .github/linux_release_builder
#
#   # AMD: bin/umcx_amd and its runtime libraries in lib/
#   docker run --rm -v $PWD:/src -e HOST_UID=$(id -u) -e HOST_GID=$(id -g) \
#       umcx-linux-release-builder bash .github/linux_release_builder/build_umcx.sh amd
#
#   # NVIDIA: bin/umcx; the HPC SDK is mounted from the host, at the same path, read-only;
#   # first make a localrc for this container's gcc, then mount it over the SDK's own
#   NV=/opt/nvidia/hpc_sdk/Linux_x86_64/25.3
#   docker run --rm -v $PWD:/src -v $NV:$NV:ro -e NVHPC_ROOT=$NV -e HOST_UID=$(id -u) -e HOST_GID=$(id -g) \
#       umcx-linux-release-builder bash .github/linux_release_builder/build_umcx.sh localrc
#   docker run --rm -v $PWD:/src -v $NV:$NV:ro -v $PWD/build/nvhpc_localrc:$NV/compilers/bin/localrc:ro \
#       -e NVHPC_ROOT=$NV -e HOST_UID=$(id -u) -e HOST_GID=$(id -g) \
#       umcx-linux-release-builder bash .github/linux_release_builder/build_umcx.sh nvc
#
# Every binary and bundled library is checked at the end: none may need a glibc newer than
# MAX_GLIBC (2.28) or a libstdc++ newer than MAX_GLIBCXX (3.4.25, gcc 8), or the build fails.

set -e
cd /src
git config --global --add safe.directory '*' 2>/dev/null || true

TARGET=${1:-amd}
CC_ARCH=${CC_ARCH:-cc70,cc80,cc86,cc90,ptx}   # ptx: JIT-compiled on newer GPUs (e.g. Blackwell)
# ROCm 6.4 has OpenMP device runtimes for gfx7xx-gfx90x, gfx94x, gfx103x, gfx110x and gfx120x (not RDNA1 gfx101x)
GFX=${GFX:-gfx900,gfx906,gfx908,gfx90a,gfx942,gfx1030,gfx1031,gfx1100,gfx1101,gfx1102,gfx1200,gfx1201}
ROCM=${ROCM:-/opt/rocm}

# files written to the mounted checkout belong to the caller, not to root
cleanup() {
    [ -n "$HOST_UID" ] && chown -R "$HOST_UID:${HOST_GID:-$HOST_UID}" /src/bin /src/lib /src/build /src/src 2>/dev/null || true
}
trap cleanup EXIT

nvhpc_env() {
    [ -n "$NVHPC_ROOT" ] || { echo "NVHPC_ROOT is not set"; exit 1; }
    NVBIN=$NVHPC_ROOT/compilers/bin
    CUDAVER=$(ls "$NVHPC_ROOT/cuda" | grep -E '^[0-9]+\.[0-9]+$' | sort -V | tail -1)
    [ -n "$CUDAVER" ] || { echo "no CUDA toolkit found in $NVHPC_ROOT/cuda"; exit 1; }
    export PATH=$NVBIN:$PATH
}

case "$TARGET" in
localrc)
    nvhpc_env
    mkdir -p build
    # -o prints the localrc; it still exits non-zero failing to lock the read-only SDK folder
    "$NVBIN/makelocalrc" "$NVBIN" -o -cuda "$CUDAVER" > build/nvhpc_localrc || true
    grep -q gcc-toolset-12 build/nvhpc_localrc || { cat build/nvhpc_localrc; echo "localrc does not use gcc-toolset-12"; exit 1; }
    echo "== build/nvhpc_localrc for CUDA $CUDAVER"
    ;;

nvc)
    nvhpc_env
    grep -q gcc-toolset-12 "$NVBIN/localrc" || { echo "mount build/nvhpc_localrc over $NVBIN/localrc first"; exit 1; }
    echo "== umcx: nvc++ $(nvc++ --version | grep -oE '[0-9]+\.[0-9]+' | head -1), CUDA $CUDAVER, $CC_ARCH"
    rm -f bin/umcx
    cd src
    make clean
    # -tp=px: generic x86-64 host code, otherwise nvc++ tunes it for the build machine;
    # nvc++ has no -static-libstdc++/-static-libgcc: link the static archives ahead of the
    # shared libraries nvc++ appends, and --as-needed drops those (like mcx does for libgomp)
    make nvc CC_ARCH="$CC_ARCH,cuda$CUDAVER" USERCXXFLAGS="-tp=px" \
        USERLINKFLAGS="-Wl,--as-needed -Wl,-Bstatic -lstdc++ -lgcc_eh -Wl,-Bdynamic" 2>&1 | grep -vE 'Generating|Loop |Minfo|^ +[0-9]+, ' || true
    cd ..
    [ -f bin/umcx ] || { echo "nvc++ build failed"; exit 1; }
    strip bin/umcx
    # nvc++ always adds libgcc_s.so.1 and an RUNPATH to the build machine's SDK; nothing is
    # imported from either (the CUDA driver is dlopen-ed), so both are removed
    if ! nm -D --undefined-only bin/umcx | grep -q '@GCC_'; then
        patchelf --remove-needed libgcc_s.so.1 bin/umcx
    fi
    patchelf --remove-rpath bin/umcx
    ;;

amd)
    echo "== umcx_amd: $("$ROCM/llvm/bin/clang++" --version | head -1), $GFX"
    rm -rf bin/umcx_amd lib
    cd src
    make clean
    make amdclang AMDCXX="$ROCM/llvm/bin/clang++" GFX="$GFX" BINARY=umcx_amd USERLINKFLAGS="-static-libstdc++ -static-libgcc"
    cd ..
    strip bin/umcx_amd

    # bundle the OpenMP offloading runtime (libomp, libomptarget) and the ROCm user-space
    # driver (HSA) with what they need beyond a base system: umcx_amd then runs with only the
    # amdgpu kernel driver, and on the CPU without a GPU; glibc, libstdc++, zlib stay system's
    mkdir -p lib
    ldd bin/umcx_amd | awk '/=> \//{print $1, $3}' | while read -r soname path; do
        if ! echo "$soname" | grep -qE '^(libc|libm|libdl|libpthread|librt|libstdc\+\+|libgcc_s|libz|libzstd)\.so|^ld-linux'; then
            cp -L "$path" "lib/$soname"
        fi
    done
    strip --strip-unneeded lib/*
    for f in lib/*; do patchelf --set-rpath '$ORIGIN' "$f"; done
    patchelf --set-rpath '$ORIGIN/../lib' bin/umcx_amd
    ls -la lib
    ;;

*)
    echo "usage: build_umcx.sh localrc|nvc|amd"
    exit 1
    ;;
esac

# smoke test: no GPU in the container, both binaries fall back to the host
if [ "$TARGET" != "localrc" ]; then
    BIN=bin/umcx; [ "$TARGET" = "amd" ] && BIN=bin/umcx_amd
    echo "== $BIN, CPU fallback"
    ldd $BIN
    (cd bin && OMP_NUM_THREADS=4 ./$(basename $BIN) --bench cube60 -n 1e4 -S 0 -d 0)
    bash .github/linux_release_builder/check_glibc.sh $BIN lib/*
fi
