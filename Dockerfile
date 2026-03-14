# syntax=docker/dockerfile:1
#
# Pintos (Threads project) Docker image.
#
# Typical usage from this repo root:
#   docker compose run --rm app bash
#   cd src/threads && make clean && make check

FROM ubuntu:22.04 AS final

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + 32-bit build support + Pintos scripts deps + simulators.
RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-multilib \
    libc6-dev-i386 \
    make \
    perl \
    python3 \
    git \
    ca-certificates \
    qemu-system-x86 \
    bochs \
    bochs-x \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /work/pintos

# Bake the repo into the image for convenience (compose bind-mount still recommended for dev).
COPY . /work/pintos

# Ensure Pintos helper scripts are executable in-container.
RUN chmod +x /work/pintos/src/utils/pintos* /work/pintos/src/utils/backtrace || true

# Make `pintos` available without editing PATH manually.
ENV PATH="/work/pintos/src/utils:${PATH}"

# In this repo, Bochs often fails due to version/config mismatches.
# Default to QEMU so `make check` works without extra flags inside Docker.
ENV SIMULATOR="--qemu"

CMD ["bash"]
