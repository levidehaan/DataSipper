#!/bin/bash

# DataSipper Universal Development Setup Script
# Automatically detects OS and uses appropriate dependency installer

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m'

# Fancy header
echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════╗
║                  DataSipper Universal Development Setup              ║
║             Network Data Interception for Chromium (Multi-OS)        ║
╚═══════════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo -e "${BLUE}Project Root: ${PROJECT_ROOT}${NC}"
echo ""

# Detect operating system
detect_os() {
    if [[ -f /etc/os-release ]]; then
        . /etc/os-release
        OS_ID="$ID"
        OS_VERSION="$VERSION_ID"
        OS_NAME="$PRETTY_NAME"
    else
        echo -e "${RED}Cannot detect operating system${NC}"
        exit 1
    fi
    
    echo -e "${YELLOW}Detected OS: ${OS_NAME}${NC}"
}

# Select appropriate dependency installer
select_dependency_installer() {
    case "$OS_ID" in
        "arch")
            DEPS_SCRIPT="install-deps-arch.sh"
            PACKAGE_MANAGER="pacman"
            ;;
        "ubuntu"|"debian")
            DEPS_SCRIPT="install-deps-debian.sh"
            PACKAGE_MANAGER="apt"
            ;;
        "fedora"|"rhel"|"centos")
            echo -e "${YELLOW}Red Hat-based systems detected but not yet supported${NC}"
            echo -e "${YELLOW}You may need to install dependencies manually${NC}"
            DEPS_SCRIPT=""
            PACKAGE_MANAGER="dnf/yum"
            ;;
        *)
            echo -e "${YELLOW}Unknown OS: $OS_ID${NC}"
            echo -e "${YELLOW}You may need to install dependencies manually${NC}"
            DEPS_SCRIPT=""
            PACKAGE_MANAGER="unknown"
            ;;
    esac
    
    echo -e "${BLUE}Package Manager: ${PACKAGE_MANAGER}${NC}"
    if [[ -n "$DEPS_SCRIPT" ]]; then
        echo -e "${BLUE}Using dependency script: ${DEPS_SCRIPT}${NC}"
    fi
}

# Command line options
SKIP_BUILD=false
BUILD_TYPE="dev"
FORCE_INSTALL=false
SKIP_DEPS=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --skip-build)
            SKIP_BUILD=true
            shift
            ;;
        --skip-deps)
            SKIP_DEPS=true
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
            echo "  --skip-deps          Skip dependency installation"
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
echo "  Skip Dependencies: $SKIP_DEPS"
echo "  Force Install: $FORCE_INSTALL"
echo ""

# Phase 1: OS Detection and Setup
echo -e "${PURPLE}=== Phase 1: System Detection ===${NC}"
detect_os
select_dependency_installer
echo ""

# Phase 2: System Requirements Check
echo -e "${PURPLE}=== Phase 2: System Requirements Check ===${NC}"

# Check disk space (need 100GB+)
AVAILABLE_SPACE=$(df -BG "$PROJECT_ROOT" | awk 'NR==2{print $4}' | sed 's/G//')
if [[ "$AVAILABLE_SPACE" -lt 100 ]]; then
    echo -e "${RED}✗ Insufficient disk space: ${AVAILABLE_SPACE}GB available, 100GB+ required${NC}"
    exit 1
fi

# Check memory (need 8GB+)
TOTAL_MEM=$(free -g | awk '/^Mem:/{print $2}')
if [[ "$TOTAL_MEM" -lt 8 ]]; then
    echo -e "${YELLOW}⚠ Warning: ${TOTAL_MEM}GB RAM detected, 8GB+ recommended${NC}"
    echo "Consider adding swap space for compilation"
fi

echo -e "${GREEN}✓ System requirements check passed${NC}"
echo "  OS: $OS_NAME"
echo "  Disk Space: ${AVAILABLE_SPACE}GB"
echo "  Memory: ${TOTAL_MEM}GB"
echo "  CPU Cores: $(nproc)"
echo ""

# Phase 3: Install Dependencies
if [[ "$SKIP_DEPS" == false ]]; then
    echo -e "${PURPLE}=== Phase 3: Install Dependencies ===${NC}"
    
    if [[ -n "$DEPS_SCRIPT" ]] && [[ -f "$SCRIPT_DIR/$DEPS_SCRIPT" ]]; then
        if [[ "$FORCE_INSTALL" == true ]] || ! command -v ninja &> /dev/null; then
            echo -e "${BLUE}Installing dependencies using $DEPS_SCRIPT...${NC}"
            bash "$SCRIPT_DIR/$DEPS_SCRIPT"
        else
            echo -e "${GREEN}✓ Dependencies appear to be installed${NC}"
            echo "  Use --force to reinstall"
        fi
    else
        echo -e "${YELLOW}⚠ No dependency script available for $OS_ID${NC}"
        echo -e "${YELLOW}Please install Chromium build dependencies manually:${NC}"
        echo ""
        case "$PACKAGE_MANAGER" in
            "dnf/yum")
                echo "  sudo dnf groupinstall \"Development Tools\""
                echo "  sudo dnf install git python3 nodejs npm ninja-build"
                echo "  # Add more Chromium-specific dependencies"
                ;;
            *)
                echo "  Install: git, python3, nodejs, npm, ninja-build"
                echo "  Install: build tools and Chromium dependencies"
                ;;
        esac
        echo ""
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${RED}Setup aborted${NC}"
            exit 1
        fi
    fi
else
    echo -e "${YELLOW}Skipping dependency installation${NC}"
fi
echo ""

# Phase 4: Setup Environment
echo -e "${PURPLE}=== Phase 4: Setup Development Environment ===${NC}"

# Check if depot_tools exists
DEPOT_TOOLS_DIR="${PROJECT_ROOT}/build/depot_tools"
if [[ ! -d "$DEPOT_TOOLS_DIR" ]]; then
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

# Phase 5: Fetch Chromium Source (Optional in this environment)
echo -e "${PURPLE}=== Phase 5: Chromium Source Status ===${NC}"

CHROMIUM_SRC="${PROJECT_ROOT}/chromium-src/src"
if [[ ! -d "$CHROMIUM_SRC" ]]; then
    echo -e "${YELLOW}⚠ Chromium source not present${NC}"
    echo -e "${BLUE}To fetch Chromium source (requires ~10GB download and ~100GB disk):${NC}"
    echo "  ./scripts/fetch-chromium.sh"
    echo ""
    echo -e "${YELLOW}Note: This may take 30-60 minutes depending on connection${NC}"
    
    if [[ -t 0 ]]; then  # Only ask if running interactively
        read -p "Fetch Chromium source now? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            echo -e "${BLUE}Fetching Chromium source...${NC}"
            bash "${SCRIPT_DIR}/fetch-chromium.sh"
        else
            echo -e "${YELLOW}Skipping Chromium fetch - can be done later${NC}"
        fi
    else
        echo -e "${YELLOW}Non-interactive mode - skipping Chromium fetch${NC}"
        echo -e "${BLUE}Run './scripts/fetch-chromium.sh' manually when ready${NC}"
    fi
else
    echo -e "${GREEN}✓ Chromium source already exists${NC}"
fi
echo ""

# Phase 6: Patch Status
echo -e "${PURPLE}=== Phase 6: DataSipper Patches Status ===${NC}"

echo -e "${BLUE}Checking patch system status...${NC}"
python3 "${PROJECT_ROOT}/scripts/patches.py" status

if [[ -d "$CHROMIUM_SRC" ]]; then
    cd "$CHROMIUM_SRC"
    if [[ ! -f ".datasipper_patches_applied" ]]; then
        echo -e "${YELLOW}⚠ DataSipper patches not yet applied${NC}"
        echo -e "${BLUE}To apply patches after fetching Chromium:${NC}"
        echo "  cd chromium-src/src"
        echo "  python3 ../../scripts/patches.py apply"
        echo ""
    else
        echo -e "${GREEN}✓ DataSipper patches already applied${NC}"
    fi
else
    echo -e "${YELLOW}Patch application will be available after Chromium source is fetched${NC}"
fi
echo ""

# Phase 7: Build Configuration
echo -e "${PURPLE}=== Phase 7: Build System Status ===${NC}"

if [[ -d "$CHROMIUM_SRC" ]]; then
    echo -e "${BLUE}Chromium source available - build system ready${NC}"
    echo -e "${BLUE}To configure build:${NC}"
    echo "  ./scripts/configure-build.sh $BUILD_TYPE"
    echo ""
    echo -e "${BLUE}To build DataSipper:${NC}"
    echo "  ninja -C chromium-src/src/out/DataSipper chrome"
else
    echo -e "${YELLOW}Build system will be available after Chromium source is fetched${NC}"
fi

# Phase 8: Summary and Next Steps
echo ""
echo -e "${CYAN}"
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════╗
║                    DataSipper Setup Summary                          ║
╚═══════════════════════════════════════════════════════════════════════╝
EOF
echo -e "${NC}"

echo -e "${GREEN}✓ Universal development environment setup completed${NC}"
echo ""
echo -e "${YELLOW}System Information:${NC}"
echo "  OS: $OS_NAME"
echo "  Package Manager: $PACKAGE_MANAGER"
echo "  Project Root: $PROJECT_ROOT"
echo ""

echo -e "${YELLOW}Available Commands:${NC}"
echo "  Environment setup:     source scripts/setup-env.sh"
echo "  Fetch Chromium:        ./scripts/fetch-chromium.sh"
echo "  Apply patches:         python3 scripts/patches.py apply"
echo "  Configure build:       ./scripts/configure-build.sh dev"
echo "  Build DataSipper:      ninja -C chromium-src/src/out/DataSipper chrome"
echo ""

echo -e "${YELLOW}Development Workflow:${NC}"
echo "  1. Fetch Chromium source (if not done): ./scripts/fetch-chromium.sh"
echo "  2. Apply DataSipper patches: cd chromium-src/src && python3 ../../scripts/patches.py apply"
echo "  3. Configure build: ./scripts/configure-build.sh dev"
echo "  4. Build: ninja -C chromium-src/src/out/DataSipper chrome"
echo "  5. Test: ./chromium-src/src/out/DataSipper/chrome"
echo ""

echo -e "${YELLOW}Quick Test (without full Chromium):${NC}"
echo "  Test patches:          python3 scripts/patches.py validate"
echo "  Test environment:      source scripts/setup-env.sh && which gclient"
echo ""

if [[ "$SKIP_BUILD" == false ]] && [[ -d "$CHROMIUM_SRC" ]]; then
    echo -e "${BLUE}Ready for build process!${NC}"
else
    echo -e "${BLUE}Environment prepared - fetch Chromium source when ready${NC}"
fi

echo -e "${GREEN}Happy coding! 🚀${NC}"