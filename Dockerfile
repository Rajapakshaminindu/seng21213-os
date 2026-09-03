# =============================================================================
# SENG21213-OS :: Docker Build Environment
# =============================================================================
# This Dockerfile creates a consistent, reproducible build environment that
# works on Windows, macOS, and Linux.
#
# USAGE
#   Build the image (once):
#     docker build -t seng21213-os-builder .
#
#   Build the OS (from your project directory):
#     docker run --rm -v "$(pwd)":/os seng21213-os-builder
#
#   To run QEMU with display (Linux with X11):
#     docker run --rm -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix \
#                -v "$(pwd)":/os seng21213-os-builder make run
#
#   Windows/macOS: Copy the .img file out and open with QEMU installed locally.
# =============================================================================

FROM ubuntu:22.04

LABEL maintainer="SENG21213 Course Team"
LABEL description="Cross-compilation environment for SENG21213-OS"

ENV DEBIAN_FRONTEND=noninteractive

# Install all required build tools
RUN apt-get update && apt-get install -y \
    # Assembler
    nasm \
    # C compiler with 32-bit multilib support
    gcc \
    gcc-multilib \
    # Linker and binary utilities
    binutils \
    # QEMU emulator
    qemu-system-x86 \
    # General utilities
    make \
    git \
    xxd \
    && rm -rf /var/lib/apt/lists/*

# Set working directory to the mounted OS source
WORKDIR /os

# Default command: build the OS image
CMD ["make", "all"]
