#!/bin/bash
# DataSipper Docker Build Controller
# This script manages the Docker build process and provides clean logging

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCKER_DIR="$PROJECT_ROOT/docker"
LOG_DIR="$PROJECT_ROOT/build-logs"

# Create log directory
mkdir -p "$LOG_DIR"

log() {
    echo -e "${GREEN}[$(date '+%H:%M:%S')] $1${NC}"
}

error() {
    echo -e "${RED}[$(date '+%H:%M:%S')] ERROR: $1${NC}"
}

info() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')] $1${NC}"
}

warning() {
    echo -e "${YELLOW}[$(date '+%H:%M:%S')] WARNING: $1${NC}"
}

log "DataSipper Docker Build Controller"
log "=================================="

# Check Docker availability
if ! command -v docker &> /dev/null; then
    error "Docker is not installed or not in PATH"
    exit 1
fi

# Check Docker daemon
if ! docker info &> /dev/null; then
    error "Docker daemon is not running"
    exit 1
fi

# Build the Docker image
DOCKER_IMAGE="datasipper-chromium-builder"
log "Building Docker image: $DOCKER_IMAGE"

if docker build -f "$DOCKER_DIR/Dockerfile.chromium-builder" -t "$DOCKER_IMAGE" "$PROJECT_ROOT" 2>&1 | tee "$LOG_DIR/docker-build.log"; then
    log "Docker image built successfully"
else
    error "Docker image build failed"
    error "Check $LOG_DIR/docker-build.log for details"
    exit 1
fi

# Run the build in Docker with proper logging
CONTAINER_NAME="datasipper-build-$(date +%s)"
LOG_FILE="$LOG_DIR/chromium-build-$(date +%Y%m%d-%H%M%S).log"

log "Starting Chromium build in Docker container: $CONTAINER_NAME"
log "Build logs will be saved to: $LOG_FILE"

# Make the build script executable
chmod +x "$DOCKER_DIR/build-chromium.sh"

# Create host directories for persistence
HOST_CHROMIUM_DIR="$PROJECT_ROOT/chromium-src"
HOST_DEPOT_TOOLS="$PROJECT_ROOT/build/depot_tools"

mkdir -p "$HOST_CHROMIUM_DIR"
mkdir -p "$HOST_DEPOT_TOOLS"
mkdir -p "$LOG_DIR"

log "Mounting directories for persistence:"
info "- Chromium source: $HOST_CHROMIUM_DIR"
info "- Depot tools: $HOST_DEPOT_TOOLS"
info "- Build logs: $LOG_DIR"

# Run the container with proper resource limits and volume mounts
docker run \
    --name "$CONTAINER_NAME" \
    --rm \
    --memory="8g" \
    --cpus="$(nproc)" \
    --volume "$DOCKER_DIR/build-chromium.sh:/home/builder/build-chromium.sh:ro" \
    --volume "$HOST_CHROMIUM_DIR:/home/builder/chromium-build:rw" \
    --volume "$HOST_DEPOT_TOOLS:/home/builder/depot_tools:rw" \
    --volume "$LOG_DIR:/home/builder/logs:rw" \
    --volume "$PROJECT_ROOT/patches:/home/builder/datasipper-patches:ro" \
    --workdir "/home/builder" \
    "$DOCKER_IMAGE" \
    bash /home/builder/build-chromium.sh 2>&1 | tee "$LOG_FILE"

BUILD_EXIT_CODE=${PIPESTATUS[0]}

# Extract logs from container if it still exists
if docker ps -a --format '{{.Names}}' | grep -q "$CONTAINER_NAME" 2>/dev/null; then
    log "Extracting logs from container..."
    
    # Copy build logs
    docker cp "$CONTAINER_NAME:/home/builder/build.log" "$LOG_DIR/build-full.log" 2>/dev/null || true
    docker cp "$CONTAINER_NAME:/home/builder/build-errors.log" "$LOG_DIR/build-errors.log" 2>/dev/null || true
    docker cp "$CONTAINER_NAME:/home/builder/build-summary.log" "$LOG_DIR/build-summary.log" 2>/dev/null || true
fi

# Analyze the results
log "Analyzing build results..."

if [ $BUILD_EXIT_CODE -eq 0 ]; then
    log "BUILD SUCCESSFUL!"
    info "Chrome has been built successfully in Docker"
    
    if [ -f "$LOG_DIR/build-full.log" ]; then
        SUCCESS_LINE=$(grep "SUCCESS:" "$LOG_DIR/build-full.log" | tail -1)
        if [ -n "$SUCCESS_LINE" ]; then
            info "$SUCCESS_LINE"
        fi
    fi
    
    log "Next steps:"
    info "1. Extract the binary from a new container"
    info "2. Apply DataSipper patches"
    info "3. Test the browser functionality"
    
else
    error "BUILD FAILED (exit code: $BUILD_EXIT_CODE)"
    
    # Show error summary
    if [ -f "$LOG_DIR/build-summary.log" ]; then
        warning "Build Error Summary:"
        cat "$LOG_DIR/build-summary.log"
    fi
    
    # Show recent errors
    if [ -f "$LOG_DIR/build-errors.log" ]; then
        warning "Recent build errors:"
        tail -10 "$LOG_DIR/build-errors.log"
    fi
    
    error "Detailed logs available at:"
    error "- Main log: $LOG_FILE"
    error "- Full build log: $LOG_DIR/build-full.log"
    error "- Error summary: $LOG_DIR/build-summary.log"
fi

# Cleanup
log "Cleaning up temporary containers..."
docker ps -a --filter "name=datasipper-build-" --format "{{.Names}}" | xargs -r docker rm -f 2>/dev/null || true

log "Build process completed. Logs saved to: $LOG_DIR"

exit $BUILD_EXIT_CODE