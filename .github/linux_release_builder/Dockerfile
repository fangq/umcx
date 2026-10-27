# Release builder for the portable Linux binaries of umcx:
#
#   bin/umcx      nvc++ (NVIDIA HPC SDK), OpenMP offloading to NVIDIA GPUs
#   bin/umcx_amd  ROCm clang++, OpenMP offloading to AMD GPUs; its runtime is bundled in lib/
#
# manylinux_2_28 is AlmaLinux 8, glibc 2.28: the binaries run on any distribution newer than
# that (RHEL/Rocky 8+, Ubuntu 18.10+, Debian 10+). manylinux2014 (glibc 2.17), which mcx and
# mcxcl use, can not be used here: the GPU code generator of nvc++ needs glibc 2.26, and ROCm
# no longer supports CentOS 7.
#
# The NVIDIA HPC SDK (17 GB installed) is too large for an image layer: it is installed on the
# runner and mounted read-only into the container, at the same path. nvc++ reads the gcc paths
# from bin/localrc, which must be one made inside this container (build_umcx.sh localrc) and
# mounted over the runner's.
#
# Run with "docker run", not as a job container, like mcx: the JavaScript actions need a newer
# node than this image can provide.

FROM quay.io/pypa/manylinux_2_28_x86_64

ARG WITH_ROCM=1
ARG ROCM_VERSION=6.4

# gcc 12: the gcc-toolset-14 default of the image is newer than nvc++ 24.x/25.x accepts;
# libstdc++-static: ROCm clang links against the system gcc 8 and libstdc++ is linked statically
RUN dnf install -y --enablerepo=powertools gcc-toolset-12-gcc-c++ libstdc++-static make zip which file \
    && dnf clean all

ENV PATH=/opt/rh/gcc-toolset-12/root/usr/bin:${PATH}

# EPEL carries an older, incompatible rocm-device-libs: install from the ROCm repository only
RUN if [ "${WITH_ROCM}" = "1" ]; then \
        printf '[rocm]\nname=ROCm %s\nbaseurl=https://repo.radeon.com/rocm/rhel8/%s/main\nenabled=1\ngpgcheck=1\ngpgkey=https://repo.radeon.com/rocm/rocm.gpg.key\n' \
            "${ROCM_VERSION}" "${ROCM_VERSION}" > /etc/yum.repos.d/rocm.repo \
        && dnf install -y --disablerepo=epel rocm-llvm rocm-device-libs openmp-extras-devel hsa-rocr-devel \
        && dnf clean all \
        && /opt/rocm/llvm/bin/clang++ --version; \
    fi

WORKDIR /src
