#!/bin/bash

# DataSipper SAFE Development Setup Script
# This version is designed to NOT close your terminal on errors

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Error handling function
handle_error() {
    echo -e "${RED}✗ Error occurred: $1${NC}"
    echo -e "${YELLOW}Continuing with next step...${NC}"
    return 0
}

# Safe command execution
safe_run() {
    echo -e "${BLUE}Running: $1${NC}"
    if eval "$1"; then
        echo -e "${GREEN}✓ Success${NC}"
        return 0
    else
        handle_error "Command failed: $1"
        return 1
    fi
}

echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════╗
║                   DataSipper SAFE Development Setup                  ║
║                 (Terminal-friendly version)                           ║
╚═══════════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${BLUE}Project Root: ${PROJECT_ROOT}${NC}"
echo ""

# Parse command line arguments
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

AVAILABLE_SPACE=$(df -BG "$PROJECT_ROOT" | awk 'NR==2{print $4}' | sed 's/G//')
TOTAL_MEM=$(free -g | awk '/^Mem:/{print $2}')

if [ "$AVAILABLE_SPACE" -lt 100 ]; then
    handle_error "Insufficient disk space: ${AVAILABLE_SPACE}GB available, 100GB+ required"
else
    echo -e "${GREEN}✓ Disk space check passed: ${AVAILABLE_SPACE}GB available${NC}"
fi

if [ "$TOTAL_MEM" -lt 8 ]; then
    echo -e "${YELLOW}⚠ Warning: ${TOTAL_MEM}GB RAM detected, 8GB+ recommended${NC}"
else
    echo -e "${GREEN}✓ Memory check passed: ${TOTAL_MEM}GB available${NC}"
fi

echo ""

# Phase 2: Dependencies
echo -e "${PURPLE}=== Phase 2: Check Dependencies ===${NC}"

if command -v ninja &> /dev/null && command -v git &> /dev/null && command -v python3 &> /dev/null; then
    echo -e "${GREEN}✓ Essential tools found${NC}"
else
    echo -e "${YELLOW}⚠ Some dependencies may be missing${NC}"
    if [ "$FORCE_INSTALL" = true ]; then
        safe_run "bash \"${SCRIPT_DIR}/install-deps-arch.sh\""
    else
        echo -e "${YELLOW}Use --force to install dependencies${NC}"
    fi
fi

echo ""

# Phase 3: Setup Environment
echo -e "${PURPLE}=== Phase 3: Setup Development Environment ===${NC}"

DEPOT_TOOLS_DIR="${PROJECT_ROOT}/build/depot_tools"
if [ ! -d "$DEPOT_TOOLS_DIR" ]; then
    echo -e "${BLUE}Cloning depot_tools...${NC}"
    mkdir -p "${PROJECT_ROOT}/build"
    if git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"; then
        echo -e "${GREEN}✓ depot_tools cloned successfully${NC}"
    else
        handle_error "Failed to clone depot_tools"
    fi
else
    echo -e "${GREEN}✓ depot_tools already exists${NC}"
    echo -e "${BLUE}Updating depot_tools...${NC}"
    cd "$DEPOT_TOOLS_DIR"
    if git pull origin main 2>/dev/null || git pull 2>/dev/null; then
        echo -e "${GREEN}✓ depot_tools updated${NC}"
    else
        echo -e "${YELLOW}⚠ depot_tools update had issues (continuing anyway)${NC}"
    fi
    cd "$PROJECT_ROOT"
fi

# Setup environment variables (safely)
export PATH="${DEPOT_TOOLS_DIR}:$PATH"
export DEPOT_TOOLS_UPDATE=1
export DEPOT_TOOLS_METRICS=0

if command -v gclient &> /dev/null; then
    echo -e "${GREEN}✓ depot_tools accessible${NC}"
else
    handle_error "depot_tools not accessible in PATH"
fi

echo ""

# Phase 4: Check Chromium Source
echo -e "${PURPLE}=== Phase 4: Check Chromium Source ===${NC}"

CHROMIUM_SRC="${PROJECT_ROOT}/chromium-src/src"
if [ ! -d "$CHROMIUM_SRC" ]; then
    echo -e "${YELLOW}Chromium source not found${NC}"
    echo -e "${BLUE}To fetch Chromium source (30-60 minutes):${NC}"
    echo "  cd $PROJECT_ROOT"
    echo "  ./scripts/fetch-chromium.sh"
    echo ""
    echo -e "${YELLOW}Skipping patch application and build configuration${NC}"
    echo ""
    echo -e "${GREEN}Setup completed successfully (partial)${NC}"
    echo -e "${YELLOW}Next steps:${NC}"
    echo "  1. Run: ./scripts/fetch-chromium.sh"
    echo "  2. Run: python3 scripts/patches.py apply"
    echo "  3. Run: ./scripts/configure-build.sh $BUILD_TYPE"
    echo "  4. Build: cd chromium-src/src && ninja -C out/$BUILD_TYPE chrome"
    exit 0
else
    echo -e "${GREEN}✓ Chromium source exists${NC}"
fi

echo ""

# Phase 5: Check Patches
echo -e "${PURPLE}=== Phase 5: Check DataSipper Patches ===${NC}"

echo -e "${BLUE}Checking patch status...${NC}"
python3 "${PROJECT_ROOT}/scripts/patches.py" status

cd "$CHROMIUM_SRC"
if [ ! -f ".datasipper_patches_applied" ]; then
    echo -e "${BLUE}DataSipper patches not yet applied${NC}"
    echo -e "${YELLOW}To apply patches manually:${NC}"
    echo "  cd $CHROMIUM_SRC"
    echo "  python3 $PROJECT_ROOT/scripts/patches.py apply"
else
    echo -e "${GREEN}✓ DataSipper patches already applied${NC}"
fi

echo ""

# Phase 6: Check Build Configuration
echo -e "${PURPLE}=== Phase 6: Check Build Configuration ===${NC}"

BUILD_DIR="out/$BUILD_TYPE"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Build not yet configured${NC}"
    echo -e "${BLUE}To configure build:${NC}"
    echo "  bash $PROJECT_ROOT/scripts/configure-build.sh $BUILD_TYPE"
else
    echo -e "${GREEN}✓ Build directory exists: $BUILD_DIR${NC}"
fi

echo ""

# Phase 7: Summary
echo -e "${PURPLE}=== Setup Summary ===${NC}"
echo -e "${GREEN}✓ Safe setup completed${NC}"
echo ""
echo -e "${YELLOW}Manual steps to complete DataSipper setup:${NC}"
echo ""
echo "1. ${BLUE}Fetch Chromium (if not done):${NC}"
echo "   ./scripts/fetch-chromium.sh"
echo ""
echo "2. ${BLUE}Apply DataSipper patches:${NC}"
echo "   cd $CHROMIUM_SRC"
echo "   python3 $PROJECT_ROOT/scripts/patches.py apply"
echo ""
echo "3. ${BLUE}Configure build:${NC}"
echo "   bash $PROJECT_ROOT/scripts/configure-build.sh $BUILD_TYPE"
echo ""
echo "4. ${BLUE}Build DataSipper:${NC}"
echo "   cd $CHROMIUM_SRC"
echo "   ninja -C out/$BUILD_TYPE chrome"
echo ""
echo "5. ${BLUE}Run DataSipper:${NC}"
echo "   ./out/$BUILD_TYPE/chrome --enable-features=DataSipperNetworkInterception"
echo ""
echo -e "${GREEN}🎉 Ready to build DataSipper! 🚀${NC}"