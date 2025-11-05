#!/bin/bash
# DataSipper Incremental Build Script
# Fast incremental builds for ongoing development

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
BUILD_DIR="out/DataSipper"
SRC_DIR="$PROJECT_ROOT/chromium-src/src"

cd "$SRC_DIR"

# Check if DataSipper build exists
if [ ! -d "$BUILD_DIR" ]; then
    error "DataSipper build directory not found. Run the full build first:"
    error "  ./scripts/build-chrome-optimized.sh"
    exit 1
fi

# Function to check what's changed since last build
check_changes() {
    log "Analyzing changes since last build..."
    
    if [ -f "$BUILD_DIR/.last_build_time" ]; then
        LAST_BUILD=$(cat "$BUILD_DIR/.last_build_time")
        CHANGED_FILES=$(find . -name "*.cc" -o -name "*.cpp" -o -name "*.h" -o -name "*.gn" -o -name "*.gni" -newer "$BUILD_DIR/.last_build_time" | wc -l)
        info "Files changed since last build: $CHANGED_FILES"
        
        if [ "$CHANGED_FILES" -eq 0 ]; then
            log "No source changes detected - build may be up to date"
        elif [ "$CHANGED_FILES" -lt 10 ]; then
            info "Few changes detected - expecting fast incremental build"
        elif [ "$CHANGED_FILES" -lt 100 ]; then
            info "Moderate changes detected - expecting medium incremental build"
        else
            info "Many changes detected - may take longer"
        fi
    else
        info "First incremental build"
    fi
}

# Function to optimize for specific target
optimize_target() {
    local target="$1"
    case "$target" in
        chrome)
            JOBS=8
            LOAD_LIMIT=10
            ;;
        chrome_sandbox)
            JOBS=4
            LOAD_LIMIT=6
            ;;
        unit_tests)
            JOBS=6
            LOAD_LIMIT=8
            ;;
        *)
            JOBS=8
            LOAD_LIMIT=10
            ;;
    esac
}

# Main incremental build function
run_incremental_build() {
    local target="${1:-chrome}"
    local force="${2:-false}"
    
    check_changes
    optimize_target "$target"
    
    log "Starting incremental build for target: $target"
    info "Using $JOBS parallel jobs with load limit $LOAD_LIMIT"
    
    START_TIME=$(date +%s)
    
    # Run ninja with optimal settings for incremental builds
    if ninja -C "$BUILD_DIR" "$target" \
        -j$JOBS \
        -l$LOAD_LIMIT \
        -k0 \
        --verbose; then
        
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        MINUTES=$((DURATION / 60))
        SECONDS=$((DURATION % 60))
        
        log "Incremental build completed successfully!"
        info "Build time: ${MINUTES}m ${SECONDS}s"
        
        # Record build time
        date +%s > "$BUILD_DIR/.last_build_time"
        
        # Verify target
        if [ "$target" = "chrome" ] && [ -f "$BUILD_DIR/chrome" ]; then
            BINARY_SIZE=$(ls -lh "$BUILD_DIR/chrome" | awk '{print $5}')
            info "Chrome binary updated (${BINARY_SIZE})"
            
            # Update symlink
            ln -sf "$SRC_DIR/$BUILD_DIR/chrome" "$PROJECT_ROOT/datasipper"
            info "Symlink updated: $PROJECT_ROOT/datasipper"
        fi
        
    else
        error "Incremental build failed"
        
        # Suggest recovery options
        info "Recovery options:"
        info "1. Try again: $0 $target"
        info "2. Clean specific files: ninja -C $BUILD_DIR -t clean $target"
        info "3. Full rebuild: ./scripts/build-chrome-optimized.sh"
        
        exit 1
    fi
}

# Function to clean specific targets
clean_target() {
    local target="$1"
    log "Cleaning target: $target"
    ninja -C "$BUILD_DIR" -t clean "$target"
    info "Target cleaned. Run incremental build to rebuild."
}

# Function to show build status
show_status() {
    log "Build status for DataSipper configuration:"
    
    if [ -f "$BUILD_DIR/chrome" ]; then
        local size=$(ls -lh "$BUILD_DIR/chrome" | awk '{print $5}')
        local modified=$(stat -c %y "$BUILD_DIR/chrome" 2>/dev/null || stat -f %Sm "$BUILD_DIR/chrome")
        info "Chrome binary: $size (modified: $modified)"
    else
        info "Chrome binary: Not found"
    fi
    
    if [ -f "$BUILD_DIR/.last_build_time" ]; then
        local last_build=$(date -d @$(cat "$BUILD_DIR/.last_build_time") 2>/dev/null || date -r $(cat "$BUILD_DIR/.last_build_time"))
        info "Last build: $last_build"
    fi
    
    # Check for common test targets
    for target in unit_tests browser_tests; do
        if [ -f "$BUILD_DIR/$target" ]; then
            info "$target: Available"
        fi
    done
    
    # Disk usage
    local build_size=$(du -sh "$BUILD_DIR" 2>/dev/null | cut -f1)
    info "Build directory size: $build_size"
}

# Main script logic
case "${1:-build}" in
    build|chrome)
        run_incremental_build chrome "${2:-false}"
        ;;
    clean)
        clean_target "${2:-chrome}"
        ;;
    status)
        show_status
        ;;
    tests)
        run_incremental_build unit_tests
        ;;
    browser_tests)
        run_incremental_build browser_tests
        ;;
    help|--help|-h)
        echo "DataSipper Incremental Build Script"
        echo "Usage: $0 [command] [options]"
        echo ""
        echo "Commands:"
        echo "  build, chrome    - Build Chrome (default)"
        echo "  tests           - Build unit tests"
        echo "  browser_tests   - Build browser tests"
        echo "  clean [target]  - Clean specific target"
        echo "  status          - Show build status"
        echo "  help            - Show this help"
        echo ""
        echo "Examples:"
        echo "  $0                    # Build Chrome incrementally"
        echo "  $0 tests              # Build unit tests"
        echo "  $0 clean chrome       # Clean Chrome target"
        echo "  $0 status             # Show build status"
        ;;
    *)
        # Assume it's a custom target
        run_incremental_build "$1" "${2:-false}"
        ;;
esac