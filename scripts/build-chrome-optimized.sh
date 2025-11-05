#!/bin/bash
# DataSipper Ultra-Fast Chrome Build - Optimized for Ryzen 5 4600G
# This script creates the fastest possible Chrome build for development

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

log "DataSipper Network Monitoring Browser Build"
log "==========================================="
log "Building custom Chromium with network interception capabilities"
log "Hardware: AMD Ryzen 5 4600G (12 threads), 125GB RAM, NVMe"
log "Target time: 45-75 minutes (first build), 5-15 minutes (incremental)"

# Setup environment
export PATH="$PROJECT_ROOT/build/depot_tools:$PATH"
cd "$PROJECT_ROOT/chromium-src/src"

# Verify depot_tools
if ! command -v gn &> /dev/null; then
    error "depot_tools not found. Run scripts/setup-env.sh first"
    exit 1
fi

# DataSipper build configuration - network monitoring and data pipeline browser
BUILD_CONFIG="
# Core build settings for DataSipper development
is_debug=false
is_component_build=true
symbol_level=0
is_official_build=false

# DataSipper network monitoring features (REQUIRED)
enable_websockets=true
enable_webrtc=true
enable_basic_printing=true
use_cups=true
proprietary_codecs=true
ffmpeg_branding=\"Chrome\"

# DataSipper external integration capabilities
# Required for Kafka, Redis, MySQL, Webhook connectors
enable_sql_database=true
enable_extensions_api=true
enable_app_list=false

# Network stack features for interception
enable_spdy=true
enable_http_cache=true
enable_background_fetch=true

# JavaScript/V8 features for custom processing scripts
v8_use_external_startup_data=true
v8_enable_javascript_promise_hooks=true

# WebUI features for slide-out configuration panel
optimize_webui=false
enable_webui_tab_strip=true

# Compilation optimizations for fast builds
use_jumbo_build=true
enable_precompiled_headers=true
use_thin_lto=false
use_cfi=false

# Disable features not needed for DataSipper
enable_nacl=false
enable_remoting=false
enable_print_preview=false
enable_pdf=false
enable_plugins=false
enable_widevine=false
enable_one_click_signin=false
enable_google_now=false
enable_service_discovery=false
enable_wifi_bootstrapping=false
enable_media_router=false

# Performance settings
treat_warnings_as_errors=false
enable_iterator_debugging=false

# Clang optimizations
is_clang=true
clang_use_chrome_plugins=true
use_lld=true

# Debug optimizations
enable_profiling=false
exclude_unwind_tables=true
enable_frame_pointers=false

# Platform specific
use_goma=false
use_siso=false
"

BUILD_DIR="out/DataSipper"
log "Using DataSipper build configuration optimized for network monitoring"

# Check if DataSipper patches are applied
if [ ! -f ".datasipper_patches_applied" ]; then
    info "DataSipper patches not detected!"
    info "This will build vanilla Chrome without network monitoring features."
    info "To apply DataSipper patches first, run:"
    info "  cd $PROJECT_ROOT && python3 scripts/patches.py apply"
    info ""
    read -p "Continue building vanilla Chrome? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        error "Build cancelled. Apply DataSipper patches first for full functionality."
        exit 1
    fi
    log "Building vanilla Chrome (without DataSipper network monitoring)"
else
    log "DataSipper patches detected - building with network monitoring capabilities"
fi

# Clean build directory if it exists and has issues
if [ -d "$BUILD_DIR" ]; then
    log "Cleaning existing build directory..."
    rm -rf "$BUILD_DIR"
fi

# Generate build
log "Generating ultra-fast build configuration..."
if gn gen "$BUILD_DIR" --args="$BUILD_CONFIG" 2>&1 | tee "$LOG_DIR/gn-gen-lightning.log"; then
    log "Build configuration generated successfully"
else
    error "GN generation failed"
    exit 1
fi

# Show configuration
log "Lightning build configuration:"
gn args "$BUILD_DIR" --list --short | grep -E "(is_debug|is_component_build|symbol_level|use_jumbo_build|use_thin_lto)" | tee "$LOG_DIR/build-config-lightning.log"

# Calculate optimal job count for your system
TOTAL_CORES=12
MEMORY_GB=125

# For Ryzen 5 4600G with 125GB RAM: use 8 parallel jobs to avoid overwhelming the CPU
# This leaves headroom for system processes and prevents thermal throttling
OPTIMAL_JOBS=8
LOAD_LIMIT=10

log "Optimal build settings for your hardware:"
info "- Parallel jobs: $OPTIMAL_JOBS (out of $TOTAL_CORES cores)"
info "- Load limit: $LOAD_LIMIT"
info "- Memory available: ${MEMORY_GB}GB"
info "- Build type: Component build (much faster linking)"

START_TIME=$(date +%s)

# Ultra-optimized ninja command
log "Starting Lightning build with optimal settings..."
if ninja -C "$BUILD_DIR" chrome \
    -j$OPTIMAL_JOBS \
    -l$LOAD_LIMIT \
    -k0 \
    2>&1 | tee "$LOG_DIR/ninja-build-lightning-$(date +%Y%m%d-%H%M%S).log"; then
    
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
        log "Validating Chrome binary..."
        if timeout 10 "$BUILD_DIR/chrome" --version 2>&1 | tee "$LOG_DIR/chrome-version-lightning.log"; then
            log "Chrome binary validation successful"
        else
            error "Chrome binary failed validation (may be normal for some builds)"
        fi
        
        # Create symlink for easy access
        ln -sf "$PROJECT_ROOT/chromium-src/src/$BUILD_DIR/chrome" "$PROJECT_ROOT/datasipper"
        log "Symlink created: $PROJECT_ROOT/datasipper"
        
    else
        error "Chrome binary not found"
        exit 1
    fi
else
    error "Build failed"
    exit 1
fi

log "DataSipper build completed successfully!"
info "Next steps:"
info "1. Test: $PROJECT_ROOT/datasipper --version"
info "2. Run DataSipper: $PROJECT_ROOT/datasipper"
info "3. For incremental builds: ninja -C $BUILD_DIR chrome -j$OPTIMAL_JOBS"
info "4. Incremental builds should take 5-15 minutes"
info ""
if [ -f ".datasipper_patches_applied" ]; then
    info "DataSipper Features Available:"
    info "- Real-time network traffic monitoring"
    info "- HTTP/HTTPS request/response capture"
    info "- WebSocket message interception"
    info "- Slide-out configuration panel"
    info "- Stream routing with condition-based filtering"
    info ""
    info "External Integration Connectors:"
    info "- Kafka Producer (topics/partitions)"
    info "- Redis (SET/HSET/LPUSH/RPUSH/SADD/ZADD/PUBLISH/XADD/INCR/APPEND)"
    info "- MySQL database storage"
    info "- HTTP webhooks"
    info "- JavaScript custom processing API"
else
    info "Note: This is vanilla Chrome. Apply DataSipper patches for:"
    info "  - Network monitoring and interception"
    info "  - Data pipeline routing to external systems"
    info "  - Slide-out configuration interface"
fi