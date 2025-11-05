#!/bin/bash
# DataSipper Optimized Docker Build Strategy
# Designed to avoid volume mounting overhead while preserving source

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${GREEN}[$(date '+%H:%M:%S')] $1${NC}"; }
error() { echo -e "${RED}[$(date '+%H:%M:%S')] ERROR: $1${NC}"; }
info() { echo -e "${BLUE}[$(date '+%H:%M:%S')] $1${NC}"; }

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"
LOG_DIR="$PROJECT_ROOT/build-logs"

mkdir -p "$LOG_DIR"

log "DataSipper Optimized Docker Build"
log "=================================="

# Check Docker
if ! command -v docker &> /dev/null; then
    error "Docker not found"
    exit 1
fi

# Performance strategy: Copy source INTO container for maximum I/O speed
# This avoids the 10-50x slowdown of volume mounts

DOCKER_IMAGE="datasipper-chromium-optimized"
CONTAINER_NAME="datasipper-build-optimized-$(date +%s)"

log "Building optimized Docker image..."
cat > "$DOCKER_DIR/Dockerfile.optimized" << 'EOF'
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive TZ=UTC

# Install all dependencies in one layer
RUN apt-get update && apt-get install -y \
    build-essential git python3 python3-pip curl wget unzip lsb-release sudo \
    pkg-config ninja-build nodejs npm gperf bison flex \
    libgtk-3-dev libgconf-2-4 libxss1 libxtst6 libxrandr2 libasound2-dev \
    libpangocairo-1.0-0 libatk1.0-0 libcairo-gobject2 libgtk-3-0 libgdk-pixbuf2.0-0 \
    libnss3-dev libglib2.0-dev libdrm2 libxcomposite1 libxdamage1 \
    && rm -rf /var/lib/apt/lists/*

# Create builder user
RUN useradd -m -s /bin/bash builder && echo 'builder ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

USER builder
WORKDIR /home/builder

# Install depot_tools
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
ENV PATH="/home/builder/depot_tools:${PATH}"

# Copy optimized build script
COPY docker/build-chromium-optimized.sh /home/builder/
RUN chmod +x /home/builder/build-chromium-optimized.sh
EOF

if docker build -f "$DOCKER_DIR/Dockerfile.optimized" -t "$DOCKER_IMAGE" "$PROJECT_ROOT" 2>&1 | tee "$LOG_DIR/docker-build-optimized.log"; then
    log "Docker image built successfully"
else
    error "Docker image build failed"
    exit 1
fi

# Strategy: Copy Chromium source into container for maximum performance
log "Starting optimized build container..."
log "Copying Chromium source for maximum I/O performance..."

# Run container and copy source
COPY_START=$(date +%s)
docker run -d --name "$CONTAINER_NAME" \
    --memory="64g" \
    --cpus="12" \
    --shm-size="16g" \
    "$DOCKER_IMAGE" \
    sleep 3600

# Copy Chromium source into container (much faster than volume mounting)
log "Copying source files into container..."
docker cp "$PROJECT_ROOT/chromium-src" "$CONTAINER_NAME:/home/builder/chromium-build"

COPY_END=$(date +%s)
COPY_TIME=$((COPY_END - COPY_START))
info "Source copy completed in ${COPY_TIME}s"

# Execute optimized build
log "Starting optimized Chrome build..."
BUILD_START=$(date +%s)

docker exec "$CONTAINER_NAME" bash /home/builder/build-chromium-optimized.sh 2>&1 | tee "$LOG_DIR/optimized-build-$(date +%Y%m%d-%H%M%S).log"

BUILD_EXIT_CODE=${PIPESTATUS[0]}
BUILD_END=$(date +%s)
BUILD_TIME=$((BUILD_END - BUILD_START))
BUILD_HOURS=$((BUILD_TIME / 3600))
BUILD_MINUTES=$(((BUILD_TIME % 3600) / 60))

if [ $BUILD_EXIT_CODE -eq 0 ]; then
    log "BUILD SUCCESSFUL!"
    info "Total build time: ${BUILD_HOURS}h ${BUILD_MINUTES}m"
    
    # Copy the built Chrome binary back
    log "Copying Chrome binary back to host..."
    mkdir -p "$PROJECT_ROOT/built-chrome"
    docker cp "$CONTAINER_NAME:/home/builder/chromium-build/src/out/Optimized/chrome" "$PROJECT_ROOT/built-chrome/"
    
    if [ -f "$PROJECT_ROOT/built-chrome/chrome" ]; then
        log "Chrome binary extracted successfully"
        info "Location: $PROJECT_ROOT/built-chrome/chrome"
        info "Size: $(ls -lh $PROJECT_ROOT/built-chrome/chrome | awk '{print $5}')"
    fi
    
    log "Next steps:"
    info "1. Test Chrome binary: $PROJECT_ROOT/built-chrome/chrome --version"
    info "2. Apply DataSipper patches"
    info "3. Rebuild with DataSipper features"
else
    error "BUILD FAILED (exit code: $BUILD_EXIT_CODE)"
fi

# Cleanup
log "Cleaning up container..."
docker rm -f "$CONTAINER_NAME" 2>/dev/null || true

log "Optimized build process completed"
exit $BUILD_EXIT_CODE