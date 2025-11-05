#!/bin/bash
# DataSipper Chromium Optimized Build Script
# Designed for high-performance machines with 128GB RAM and 12+ cores

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() {
    echo -e "${GREEN}[$(date '+%H:%M:%S')] $1${NC}" | tee -a /home/builder/build.log
}

error() {
    echo -e "${RED}[$(date '+%H:%M:%S')] ERROR: $1${NC}" | tee -a /home/builder/build.log
}

warning() {
    echo -e "${YELLOW}[$(date '+%H:%M:%S')] WARNING: $1${NC}" | tee -a /home/builder/build.log
}

info() {
    echo -e "${BLUE}[$(date '+%H:%M:%S')] INFO: $1${NC}" | tee -a /home/builder/build.log
}

# Initialize log file
echo "DataSipper Optimized Chromium Build - $(date)" > /home/builder/build.log
echo "================================================" >> /home/builder/build.log

# System information
log "High-Performance Build Configuration:"
info "OS: $(lsb_release -d | cut -f2)"
info "Memory: $(free -h | grep Mem | awk '{print $2}')"
info "CPU: $(nproc) cores"
info "Build strategy: Optimized for performance"

# Setup environment
export CHROMIUM_DIR="/home/builder/chromium-build"
cd "$CHROMIUM_DIR/src"

# Target the stable commit
TARGET_COMMIT="6d0796400dc7f4912cf196e27314fd51731de2d2"
log "Using stable commit: $TARGET_COMMIT"

# Optimized build configuration - NO DEBUG, maximum performance
BUILD_CONFIG="is_debug=false is_component_build=false symbol_level=0 enable_nacl=false treat_warnings_as_errors=false is_official_build=false use_jumbo_build=true"

log "Optimized build configuration: $BUILD_CONFIG"
log "Expected build time: 2-3 hours on this hardware"

# Clean any previous attempts
rm -rf out/Optimized
BUILD_DIR="out/Optimized"

# Generate build files
log "Generating optimized build configuration..."
if gn gen "$BUILD_DIR" --args="$BUILD_CONFIG" 2>&1 | tee -a /home/builder/build.log; then
    log "Build configuration generated successfully"
else
    error "Build configuration failed"
    exit 1
fi

# Show the configuration for verification
log "Verifying build arguments..."
gn args "$BUILD_DIR" --list --short 2>&1 | tee -a /home/builder/build.log

# Build Chrome with maximum parallelism
log "Starting Chrome build with maximum parallelism..."
log "Using $(nproc) parallel jobs"

START_TIME=$(date +%s)

# Build with optimized ninja settings
if ninja -C "$BUILD_DIR" chrome -j$(nproc) -l$(nproc) 2>&1 | tee -a /home/builder/build.log; then
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    HOURS=$((DURATION / 3600))
    MINUTES=$(((DURATION % 3600) / 60))
    
    log "BUILD SUCCESSFUL!"
    info "Build time: ${HOURS}h ${MINUTES}m"
    
    # Verify the binary exists
    if [ -f "$BUILD_DIR/chrome" ]; then
        info "Chrome binary created: $BUILD_DIR/chrome"
        info "Binary size: $(ls -lh $BUILD_DIR/chrome | awk '{print $5}')"
        
        # Test the binary
        if timeout 10 "$BUILD_DIR/chrome" --version 2>&1 | tee -a /home/builder/build.log; then
            log "Chrome binary validation successful"
        else
            warning "Chrome binary created but failed validation"
        fi
    else
        error "Chrome binary not found despite successful build"
        exit 1
    fi
else
    error "Chrome build failed"
    exit 1
fi

log "Optimized build completed successfully"
info "Next: Apply DataSipper patches and rebuild"