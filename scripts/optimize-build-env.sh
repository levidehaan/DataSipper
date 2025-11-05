#!/bin/bash
# DataSipper Build Environment Optimizer
# Configures the optimal environment for Chrome builds on this system

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log() { echo -e "${GREEN}[$(date '+%H:%M:%S')] $1${NC}"; }
error() { echo -e "${RED}[$(date '+%H:%M:%S')] ERROR: $1${NC}"; }
info() { echo -e "${BLUE}[$(date '+%H:%M:%S')] INFO: $1${NC}"; }
warning() { echo -e "${YELLOW}[$(date '+%H:%M:%S')] WARNING: $1${NC}"; }

log "Optimizing build environment for Chrome compilation"
log "=================================================="

# Check current system state
info "System analysis:"
info "- CPU: $(nproc) cores ($(cat /proc/cpuinfo | grep "model name" | head -1 | cut -d: -f2 | xargs))"
info "- Memory: $(free -h | awk '/^Mem:/ {print $2}') total, $(free -h | awk '/^Mem:/ {print $7}') available"
info "- Disk space: $(df -h /storage/projects/datasipper | tail -1 | awk '{print $4}') free"

# 1. Set up project-local temporary directory
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_TMP="$PROJECT_ROOT/tmp"

log "Setting up project temporary directory..."
mkdir -p "$PROJECT_TMP"
export TMPDIR="$PROJECT_TMP"

# Check available space in project directory
PROJECT_SPACE=$(df "$PROJECT_ROOT" | tail -1 | awk '{print $4}')
if [ "$PROJECT_SPACE" -gt 10000000 ]; then  # 10GB
    info "TMPDIR: Using $PROJECT_TMP with sufficient space"
else
    warning "TMPDIR: Limited space in project directory"
fi

# 2. Set optimal environment variables for the build
log "Configuring build environment variables..."

# Memory settings
export NINJA_STATUS="[%f/%t %o/sec] "
export NINJA_SUMMARIZE_BUILD=1

# Compiler settings for faster builds
export CC_WRAPPER=""
export CXX_WRAPPER=""
export AR_WRAPPER=""

# Disable debug assertions in release builds
export DCHECK_ALWAYS_ON=0

# Linker optimizations
export LDFLAGS="-Wl,--threads=8 -Wl,--compress-debug-sections=zlib"

# Parallel processing
export MAKEFLAGS="-j8"

# Python optimizations
export PYTHONUNBUFFERED=1
export PYTHONDONTWRITEBYTECODE=1

# Build tool optimizations
export GOMA_DISABLED=1
export USE_GOMA=false

log "Environment optimizations applied:"
info "- Ninja status reporting enabled"
info "- Parallel linking with 8 threads"
info "- Python optimizations enabled"
info "- Debug assertions disabled for release builds"

# 3. System optimizations (if running as root or with sudo)
if [ "$EUID" -eq 0 ] || command -v sudo &> /dev/null; then
    log "Applying system optimizations..."
    
    # Increase file descriptor limits
    if [ -w /etc/security/limits.conf ]; then
        if ! grep -q "chrome build optimization" /etc/security/limits.conf; then
            cat >> /etc/security/limits.conf << EOF
# Chrome build optimization
* soft nofile 65536
* hard nofile 65536
* soft nproc 32768
* hard nproc 32768
EOF
            info "Increased file descriptor limits"
        fi
    fi
    
    # Set optimal swappiness (if swap exists)
    if [ -f /proc/sys/vm/swappiness ]; then
        CURRENT_SWAPPINESS=$(cat /proc/sys/vm/swappiness)
        if [ "$CURRENT_SWAPPINESS" -gt 10 ]; then
            echo 10 | sudo tee /proc/sys/vm/swappiness > /dev/null
            info "Reduced swappiness from $CURRENT_SWAPPINESS to 10"
        fi
    fi
    
    # Optimize I/O scheduler for SSD (if using NVMe)
    for disk in /sys/block/nvme*; do
        if [ -d "$disk" ]; then
            SCHEDULER_FILE="$disk/queue/scheduler"
            if [ -w "$SCHEDULER_FILE" ]; then
                echo "none" | sudo tee "$SCHEDULER_FILE" > /dev/null
                info "Set NVMe scheduler to 'none' for better performance"
            fi
        fi
    done
    
else
    warning "Not running as root - skipping system optimizations"
    info "For best performance, consider running with sudo for system optimizations"
fi

# 4. Create optimized build aliases
PROFILE_FILE="$HOME/.bashrc"
if [ -f "$PROFILE_FILE" ]; then
    log "Creating build aliases..."
    
    # Remove old aliases
    sed -i '/# Chrome build aliases/,/# End Chrome build aliases/d' "$PROFILE_FILE"
    
    # Add new aliases
    cat >> "$PROFILE_FILE" << 'EOF'
# DataSipper build aliases
alias datasipper-build='cd /storage/projects/datasipper && ./scripts/build-chrome-optimized.sh'
alias datasipper-build-incremental='cd /storage/projects/datasipper/chromium-src/src && ninja -C out/DataSipper chrome -j8'
alias datasipper-run='cd /storage/projects/datasipper && ./datasipper'
alias datasipper-build-clean='cd /storage/projects/datasipper/chromium-src/src && rm -rf out/DataSipper'
alias datasipper-patches-apply='cd /storage/projects/datasipper && python3 scripts/patches.py apply'
# End DataSipper build aliases
EOF
    info "Build aliases added to $PROFILE_FILE"
    info "  - datasipper-build: Start optimized DataSipper build"
    info "  - datasipper-build-incremental: Quick incremental build"
    info "  - datasipper-run: Run DataSipper browser"
    info "  - datasipper-build-clean: Clean build directory"
    info "  - datasipper-patches-apply: Apply DataSipper network monitoring patches"
fi

# 5. Check for potential issues
log "Checking for potential build issues..."

# Check disk space
AVAILABLE_GB=$(df /storage/projects/datasipper | tail -1 | awk '{print $4}' | xargs -I {} echo "scale=2; {}/1024/1024" | bc)
if (( $(echo "$AVAILABLE_GB < 50" | bc -l) )); then
    warning "Low disk space: ${AVAILABLE_GB}GB available. Chrome builds need 30-50GB"
fi

# Check memory
AVAILABLE_MEM_GB=$(free -g | awk '/^Mem:/ {print $7}')
if [ "$AVAILABLE_MEM_GB" -lt 8 ]; then
    warning "Low available memory: ${AVAILABLE_MEM_GB}GB. Consider closing other applications"
fi

# Check for running Chrome/DataSipper instances
if pgrep -x chrome > /dev/null || pgrep -f datasipper > /dev/null; then
    warning "Chrome/DataSipper is currently running. Consider closing it to free up resources"
fi

log "Build environment optimization complete!"
info "You can now run the optimized DataSipper build with:"
info "  ./scripts/build-chrome-optimized.sh"
info ""
info "IMPORTANT: For full DataSipper network monitoring features:"
info "  1. Apply patches first: datasipper-patches-apply"
info "  2. Then build: datasipper-build"
info ""
info "For subsequent builds, use the incremental command:"
info "  datasipper-build-incremental"