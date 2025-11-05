#!/bin/bash

# DataSipper Complete Development Setup Script
# This script sets up the entire development environment for DataSipper

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Fancy header
echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════╗
║                        DataSipper Development Setup                  ║
║                   Network Data Interception for Chromium             ║
╚═══════════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${BLUE}Project Root: ${PROJECT_ROOT}${NC}"
echo ""

# Setup phases
PHASES=(
    "1. System Requirements Check"
    "2. Install Arch Linux Dependencies" 
    "3. Setup Development Environment"
    "4. Fetch Chromium Source Code"
    "5. Apply DataSipper Patches"
    "6. Configure Build System"
    "7. Initial Build (Optional)"
)

echo -e "${YELLOW}Setup Phases:${NC}"
for phase in "${PHASES[@]}"; do
    echo "  $phase"
done
echo ""

# Command line options
SKIP_BUILD=false
BUILD_TYPE="dev"
FORCE_INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --force)
            FORCE_INSTALL=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --skip-build         Skip the initial build step"
            echo "  --build-type TYPE    Build type: debug, release, or dev (default: dev)"
            echo "  --force              Force reinstall dependencies"
            echo "  -h, --help           Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

echo -e "${YELLOW}Configuration:${NC}"
echo "  Build Type: $BUILD_TYPE"
echo "  Skip Build: $SKIP_BUILD"
echo "  Force Install: $FORCE_INSTALL"
echo ""

# Phase 1: System Requirements Check
echo -e "${PURPLE}=== Phase 1: System Requirements Check ===${NC}"

if ! command -v pacman &> /dev/null; then
    echo -e "${RED}✗ This setup requires Arch Linux with pacman${NC}"
    exit 1
fi

# Check disk space (need 100GB+)
AVAILABLE_SPACE=$(df -BG "$PROJECT_ROOT" | awk 'NR==2{print $4}' | sed 's/G//')
if [ "$AVAILABLE_SPACE" -lt 100 ]; then
    echo -e "${RED}✗ Insufficient disk space: ${AVAILABLE_SPACE}GB available, 100GB+ required${NC}"
    exit 1
fi

# Check memory (need 8GB+)
TOTAL_MEM=$(free -g | awk '/^Mem:/{print $2}')
if [ "$TOTAL_MEM" -lt 8 ]; then
    echo -e "${YELLOW}⚠ Warning: ${TOTAL_MEM}GB RAM detected, 8GB+ recommended${NC}"
    echo "Consider adding swap space for compilation"
fi

echo -e "${GREEN}✓ System requirements check passed${NC}"
echo "  Disk Space: ${AVAILABLE_SPACE}GB"
echo "  Memory: ${TOTAL_MEM}GB"
echo ""

# Phase 2: Install Dependencies
echo -e "${PURPLE}=== Phase 2: Install Dependencies ===${NC}"

if [ "$FORCE_INSTALL" = true ] || ! command -v ninja &> /dev/null; then
    echo -e "${BLUE}Installing Arch Linux dependencies...${NC}"
    bash "${SCRIPT_DIR}/install-deps-arch.sh"
else
    echo -e "${GREEN}✓ Dependencies appear to be installed${NC}"
    echo "  Use --force to reinstall"
fi
echo ""

# Phase 3: Setup Environment
echo -e "${PURPLE}=== Phase 3: Setup Development Environment ===${NC}"

# Check if depot_tools exists
DEPOT_TOOLS_DIR="${PROJECT_ROOT}/build/depot_tools"
if [ ! -d "$DEPOT_TOOLS_DIR" ]; then
    echo -e "${BLUE}Cloning depot_tools...${NC}"
    mkdir -p "${PROJECT_ROOT}/build"
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
else
    echo -e "${GREEN}✓ depot_tools already exists${NC}"
    echo -e "${BLUE}Updating depot_tools...${NC}"
    cd "$DEPOT_TOOLS_DIR"
    git pull
fi

# Source environment
source "${SCRIPT_DIR}/setup-env.sh"

echo -e "${GREEN}✓ Development environment configured${NC}"
echo ""

# Phase 4: Fetch Chromium Source
echo -e "${PURPLE}=== Phase 4: Fetch Chromium Source ===${NC}"

CHROMIUM_SRC="${PROJECT_ROOT}/chromium-src/src"
if [ ! -d "$CHROMIUM_SRC" ]; then
    echo -e "${BLUE}Fetching Chromium source (this will take 30-60 minutes)...${NC}"
    bash "${SCRIPT_DIR}/fetch-chromium.sh"
else
    echo -e "${GREEN}✓ Chromium source already exists${NC}"
    echo -e "${YELLOW}To update: cd chromium-src/src && git pull && gclient sync${NC}"
fi
echo ""

# Phase 5: Apply DataSipper Patches
echo -e "${PURPLE}=== Phase 5: Apply DataSipper Patches ===${NC}"

# Check patch system status
echo -e "${BLUE}Checking patch system status...${NC}"
python3 "${PROJECT_ROOT}/scripts/patches.py" status

if [ -d "$CHROMIUM_SRC" ]; then
    cd "$CHROMIUM_SRC"
    if [ ! -f ".datasipper_patches_applied" ]; then
        echo -e "${BLUE}Applying DataSipper patches...${NC}"
        
        # Apply patches in series for better error handling
        echo -e "${YELLOW}Applying upstream fixes...${NC}"
        python3 "${PROJECT_ROOT}/scripts/patches.py" apply --series upstream-fixes
        
        echo -e "${YELLOW}Applying core DataSipper infrastructure...${NC}"
        python3 "${PROJECT_ROOT}/scripts/patches.py" apply --series core/datasipper
        
        echo -e "${YELLOW}Applying network interception...${NC}"
        python3 "${PROJECT_ROOT}/scripts/patches.py" apply --series core/network-interception
        
        echo -e "${YELLOW}Applying UI panel (if available)...${NC}"
        python3 "${PROJECT_ROOT}/scripts/patches.py" apply --series core/ui-panel || true
        
        echo -e "${YELLOW}Applying extra features (if available)...${NC}"
        python3 "${PROJECT_ROOT}/scripts/patches.py" apply --series extra || true
        
        # Final validation
        if [ -f ".datasipper_patches_applied" ]; then
            echo -e "${GREEN}✓ DataSipper patches applied successfully${NC}"
        else
            echo -e "${RED}✗ Failed to apply patches${NC}"
            exit 1
        fi
    else
        echo -e "${GREEN}✓ DataSipper patches already applied${NC}"
    fi
else
    echo -e "${RED}✗ Chromium source directory not found${NC}"
    echo -e "${YELLOW}Patch application skipped - run fetch-chromium.sh first${NC}"
fi
echo ""

# Phase 6: Configure Build
echo -e "${PURPLE}=== Phase 6: Configure Build System ===${NC}"

echo -e "${BLUE}Configuring $BUILD_TYPE build...${NC}"
bash "${SCRIPT_DIR}/configure-build.sh" "$BUILD_TYPE"

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build system configured successfully${NC}"
else
    echo -e "${RED}✗ Build configuration failed${NC}"
    exit 1
fi
echo ""

# Phase 7: Optional Build
if [ "$SKIP_BUILD" = false ]; then
    echo -e "${PURPLE}=== Phase 7: Initial Build ===${NC}"
    
    BUILD_DIR=""
    case "$BUILD_TYPE" in
        "debug") BUILD_DIR="out/Debug" ;;
        "release") BUILD_DIR="out/Release" ;;
        "dev") BUILD_DIR="out/DataSipper" ;;
    esac
    
    echo -e "${YELLOW}This will take 1-4 hours depending on your system${NC}"
    read -p "Start building DataSipper now? (y/N): " -n 1 -r
    echo
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${BLUE}Starting build...${NC}"
        
        # Calculate optimal job count
        JOBS=$(nproc)
        MEMORY_GB=$(free -g | awk '/^Mem:/{print $2}')
        MAX_JOBS=$((MEMORY_GB / 2))
        
        if [ $JOBS -gt $MAX_JOBS ]; then
            JOBS=$MAX_JOBS
        fi
        
        echo "Using $JOBS parallel jobs"
        ninja -C "$BUILD_DIR" -j$JOBS chrome
        
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ DataSipper build completed successfully!${NC}"
        else
            echo -e "${RED}✗ Build failed${NC}"
            echo "Check build logs above for errors"
            exit 1
        fi
    else
        echo -e "${YELLOW}Skipping build - run manually later with:${NC}"
        echo "  ninja -C $BUILD_DIR chrome"
    fi
fi

# Summary
echo ""
echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════╗
║                    DataSipper Setup Complete!                        ║
╚═══════════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo -e "${GREEN}✓ Development environment is ready${NC}"
echo ""
echo -e "${YELLOW}Quick Commands:${NC}"
echo "  Source environment:    source scripts/setup-env.sh"
echo "  Build DataSipper:      ninja -C out/$BUILD_TYPE chrome"
echo "  Run DataSipper:        ./out/$BUILD_TYPE/chrome"
echo "  Apply new patches:     python3 scripts/patches.py apply"
echo "  Update Chromium:       cd chromium-src/src && git pull && gclient sync"
echo ""

echo -e "${YELLOW}Development Workflow:${NC}"
echo "  1. Edit patches in patches/ directory"
echo "  2. Apply patches: python3 scripts/patches.py apply"
echo "  3. Build: ninja -C out/$BUILD_TYPE chrome"
echo "  4. Test: ./out/$BUILD_TYPE/chrome"
echo ""

echo -e "${YELLOW}Useful Directories:${NC}"
echo "  Project Root:      $PROJECT_ROOT"
echo "  Chromium Source:   $CHROMIUM_SRC"
echo "  DataSipper Patches: $PROJECT_ROOT/patches/"
echo "  Build Output:      $PROJECT_ROOT/chromium-src/src/out/$BUILD_TYPE/"
echo ""

if [ "$SKIP_BUILD" = true ]; then
    echo -e "${BLUE}Remember to build DataSipper before testing:${NC}"
    echo "  ninja -C out/$BUILD_TYPE chrome"
    echo ""
fi

echo -e "${GREEN}Happy coding! 🚀${NC}"