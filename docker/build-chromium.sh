#!/bin/bash
# DataSipper Chromium Build Script with comprehensive logging and error handling

set -e  # Exit on any error

# Color output for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging function
log() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')] $1${NC}" | tee -a /home/builder/build.log
}

error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ERROR: $1${NC}" | tee -a /home/builder/build.log
}

warning() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] WARNING: $1${NC}" | tee -a /home/builder/build.log
}

info() {
    echo -e "${BLUE}[$(date '+%Y-%m-%d %H:%M:%S')] INFO: $1${NC}" | tee -a /home/builder/build.log
}

# Initialize log file
echo "DataSipper Chromium Build Log - $(date)" > /home/builder/build.log
echo "=================================================" >> /home/builder/build.log

# System information
log "System Information:"
info "OS: $(lsb_release -d | cut -f2)"
info "Kernel: $(uname -r)"
info "Memory: $(free -h | grep Mem | awk '{print $2}')"
info "CPU: $(nproc) cores"
info "Disk space: $(df -h /home/builder | tail -1 | awk '{print $4}') available"

# Check depot_tools
log "Checking depot_tools installation..."
if ! command -v gn &> /dev/null; then
    error "gn not found in PATH"
    exit 1
fi

if ! command -v ninja &> /dev/null; then
    error "ninja not found in PATH"
    exit 1
fi

log "depot_tools version:"
info "$(gclient --version)"

# Fetch Chromium source with error handling
CHROMIUM_DIR="/home/builder/chromium-build"
log "Setting up Chromium source at $CHROMIUM_DIR"

cd "$CHROMIUM_DIR"

if [ ! -d "src" ]; then
    log "Fetching Chromium source (this will take a while)..."
    fetch chromium 2>&1 | tee -a /home/builder/build.log
else
    log "Chromium source already exists, updating..."
fi

cd src

# Target commit from DataSipper config
TARGET_COMMIT="6d0796400dc7f4912cf196e27314fd51731de2d2"
log "Checking out target commit: $TARGET_COMMIT"

# Try different strategies for problematic commits
git fetch origin 2>&1 | tee -a /home/builder/build.log

# Try the exact commit first
if git checkout "$TARGET_COMMIT" 2>&1 | tee -a /home/builder/build.log; then
    log "Successfully checked out target commit"
else
    warning "Target commit failed, trying alternative approaches..."
    
    # Try a slightly newer commit from the same branch
    log "Attempting to find a working commit near the target..."
    git fetch origin refs/branch-heads/7151:refs/remotes/origin/branch-heads/7151 2>&1 | tee -a /home/builder/build.log
    
    # Get commits from the same branch
    ALTERNATIVE_COMMITS=$(git log --oneline origin/branch-heads/7151 | head -10 | awk '{print $1}')
    
    for commit in $ALTERNATIVE_COMMITS; do
        info "Trying commit: $commit"
        if git checkout "$commit" 2>&1 | tee -a /home/builder/build.log; then
            log "Successfully checked out alternative commit: $commit"
            break
        fi
    done
fi

# Sync dependencies
log "Syncing dependencies..."
gclient sync --with_branch_heads --with_tags 2>&1 | tee -a /home/builder/build.log

# Build configuration testing - try multiple configurations
BUILD_CONFIGS=(
    # Configuration 1: Crash workaround build - disable problematic optimizations
    "is_debug=false is_component_build=false symbol_level=0 enable_nacl=false treat_warnings_as_errors=false use_clang_plugin=false is_official_build=false enable_iterator_debugging=false"
    
    # Configuration 2: Minimal debug build to avoid optimizer crashes
    "is_debug=true is_component_build=true symbol_level=0 enable_nacl=false treat_warnings_as_errors=false use_clang_plugin=false optimize_webui=false enable_iterator_debugging=false"
    
    # Configuration 3: Last resort - very minimal build
    "is_debug=true is_component_build=true symbol_level=0 enable_nacl=false treat_warnings_as_errors=false use_clang_plugin=false optimize_webui=false enable_iterator_debugging=false is_clang=true"
)

BUILD_SUCCESS=false

for i in "${!BUILD_CONFIGS[@]}"; do
    CONFIG="${BUILD_CONFIGS[$i]}"
    BUILD_DIR="out/Config$((i+1))"
    
    log "Attempting build configuration $((i+1)): $CONFIG"
    log "Build directory: $BUILD_DIR"
    
    # Clean previous attempts
    rm -rf "$BUILD_DIR"
    
    # Generate build files
    if gn gen "$BUILD_DIR" --args="$CONFIG" 2>&1 | tee -a /home/builder/build.log; then
        log "Build configuration $((i+1)) generated successfully"
        
        # Attempt to build base library first (quick test)
        log "Testing build with base library..."
        if timeout 600 ninja -C "$BUILD_DIR" base 2>&1 | tee -a /home/builder/build.log; then
            log "Base library built successfully with config $((i+1))"
            
            # Try building chrome
            log "Building Chrome browser..."
            NINJA_EXIT_CODE=0
            timeout 7200 ninja -C "$BUILD_DIR" chrome 2>&1 | tee -a /home/builder/build.log || NINJA_EXIT_CODE=$?
            
            if [ $NINJA_EXIT_CODE -eq 0 ]; then
                # Ninja completed, but verify the binary actually exists
                if [ -f "$BUILD_DIR/chrome" ]; then
                    log "SUCCESS: Chrome built successfully with configuration $((i+1))"
                    info "Chrome binary created at: $BUILD_DIR/chrome"
                    info "Binary size: $(ls -lh $BUILD_DIR/chrome | awk '{print $5}')"
                    BUILD_SUCCESS=true
                    
                    # Test if it can start (basic validation)
                    if timeout 10 "$BUILD_DIR/chrome" --version 2>&1 | tee -a /home/builder/build.log; then
                        log "Chrome binary validation successful"
                    else
                        warning "Chrome binary created but failed validation"
                    fi
                    break
                else
                    warning "Ninja completed but Chrome binary not found - build may have failed"
                    # Log the specific error
                    tail -50 /home/builder/build.log | grep -E "(FAILED|error|Error)" >> /home/builder/build-errors.log
                fi
            else
                warning "Chrome build failed with configuration $((i+1)) (ninja exit code: $NINJA_EXIT_CODE)"
                # Log the specific error
                tail -50 /home/builder/build.log | grep -E "(FAILED|error|Error)" >> /home/builder/build-errors.log
            fi
        else
            warning "Base library build failed with configuration $((i+1))"
        fi
    else
        error "Build configuration $((i+1)) generation failed"
    fi
    
    log "Configuration $((i+1)) completed. Success: $BUILD_SUCCESS"
    echo "---" >> /home/builder/build.log
done

# Final status
if [ "$BUILD_SUCCESS" = true ]; then
    log "BUILD SUCCESSFUL: Chrome has been built successfully"
    log "Next steps:"
    info "1. Test the binary: $BUILD_DIR/chrome --version"
    info "2. Apply DataSipper patches"
    info "3. Rebuild with DataSipper modifications"
else
    error "BUILD FAILED: All build configurations failed"
    error "Check /home/builder/build.log and /home/builder/build-errors.log for details"
    
    # Create summary of errors
    log "Creating error summary..."
    echo "=== BUILD ERROR SUMMARY ===" > /home/builder/build-summary.log
    echo "Date: $(date)" >> /home/builder/build-summary.log
    echo "Commit attempted: $TARGET_COMMIT" >> /home/builder/build-summary.log
    echo "" >> /home/builder/build-summary.log
    
    if [ -f /home/builder/build-errors.log ]; then
        echo "=== COMPILATION ERRORS ===" >> /home/builder/build-summary.log
        cat /home/builder/build-errors.log >> /home/builder/build-summary.log
    fi
    
    exit 1
fi

log "Build process completed. Logs available at:"
info "- Full log: /home/builder/build.log"
info "- Error log: /home/builder/build-errors.log"
info "- Summary: /home/builder/build-summary.log"
