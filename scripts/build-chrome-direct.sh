#!/bin/bash
# DataSipper Direct Host Build - Maximum Performance
# Build Chrome directly on host system (fastest option)

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${GREEN}[$(date '+%H:%M:%S')] $1${NC}"; }
error() { echo -e "${RED}[$(date '+%H:%M:%S')] ERROR: $1${NC}"; }
info() { echo -e "${BLUE}[$(date '+%H:%M:%S')] INFO: $1${NC}"; }

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="$PROJECT_ROOT/build-logs"
mkdir -p "$LOG_DIR"

log "DataSipper Direct Host Chrome Build"
log "===================================="
log "Hardware: 12 cores, 128GB RAM, NVMe storage"
log "Expected time: 90-120 minutes"

# Setup environment
export PATH="$PROJECT_ROOT/build/depot_tools:$PATH"
cd "$PROJECT_ROOT/chromium-src/src"

# Verify depot_tools
if ! command -v gn &> /dev/null; then
    error "depot_tools not found. Run scripts/setup-env.sh first"
    exit 1
fi

# Super optimized build configuration for maximum speed
BUILD_CONFIG="
is_debug=false
is_component_build=false
symbol_level=0
enable_nacl=false
treat_warnings_as_errors=false
is_official_build=false
use_jumbo_build=true
enable_precompiled_headers=false
is_clang=true
enable_iterator_debugging=false
optimize_webui=false
enable_remoting=false
enable_print_preview=false
"

log "Ultra-optimized build configuration for speed"
log "Using release build with jumbo compilation"

# Clean build directory
BUILD_DIR="out/UltraFast"
rm -rf "$BUILD_DIR"

# Generate build
log "Generating build files..."
if gn gen "$BUILD_DIR" --args="$BUILD_CONFIG" 2>&1 | tee "$LOG_DIR/gn-gen.log"; then
    log "Build configuration generated"
else
    error "GN generation failed"
    exit 1
fi

# Show final configuration
log "Build configuration:"
gn args "$BUILD_DIR" --list --short | grep -E "(is_debug|is_component_build|symbol_level|use_jumbo_build)" | tee "$LOG_DIR/build-config.log"

# Maximum performance build
log "Starting maximum performance build..."
log "Using all 12 CPU cores with optimal load balancing"

START_TIME=$(date +%s)

# Ultra-optimized ninja command
if ninja -C "$BUILD_DIR" chrome \
    -j12 \
    -l12 \
    -k0 \
    --verbose 2>&1 | tee "$LOG_DIR/ninja-build-$(date +%Y%m%d-%H%M%S).log"; then
    
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    HOURS=$((DURATION / 3600))
    MINUTES=$(((DURATION % 3600) / 60))
    
    log "BUILD SUCCESSFUL!"
    info "Build time: ${HOURS}h ${MINUTES}m"
    
    # Verify binary
    if [ -f "$BUILD_DIR/chrome" ]; then
        BINARY_SIZE=$(ls -lh "$BUILD_DIR/chrome" | awk '{print $5}')
        log "Chrome binary created successfully"
        info "Location: $PROJECT_ROOT/chromium-src/src/$BUILD_DIR/chrome"
        info "Size: $BINARY_SIZE"
        
        # Quick validation
        if timeout 10 "$BUILD_DIR/chrome" --version 2>&1 | tee "$LOG_DIR/chrome-version.log"; then
            log "Chrome binary validation successful"
        else
            error "Chrome binary failed validation"
        fi
        
        # Create symlink for easy access
        ln -sf "$PROJECT_ROOT/chromium-src/src/$BUILD_DIR/chrome" "$PROJECT_ROOT/chrome"
        log "Symlink created: $PROJECT_ROOT/chrome"
        
    else
        error "Chrome binary not found"
        exit 1
    fi
else
    error "Build failed"
    exit 1
fi

log "Direct host build completed successfully!"
info "Next steps:"
info "1. Test: $PROJECT_ROOT/chrome --version"
info "2. Apply DataSipper patches"
info "3. Rebuild with DataSipper features"