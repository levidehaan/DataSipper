#!/bin/bash

#############################################################
# DataSipper Development Environment Setup Script
#
# Purpose: Set up build environment for DataSipper/Chromium
# Platform: Ubuntu 24.04 (should work on most Debian-based)
# Usage: Run this script each time environment spins up
#############################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}TPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW${NC}"
echo -e "${GREEN}Q  DataSipper Development Environment Setup     Q${NC}"
echo -e "${GREEN}ZPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP]${NC}"
echo

# Get directory paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHROMIUM_SRC="${SCRIPT_DIR}/chromium-src/src"
BUILD_DIR="${SCRIPT_DIR}/build"
DEPOT_TOOLS="${BUILD_DIR}/depot_tools"

# Check if we're root (we need it for apt)
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}ERROR: This script must be run as root${NC}"
  echo "Please run with sudo or as root user"
  exit 1
fi

echo -e "${BLUE}=== Step 1: Checking System Requirements ===${NC}"

# Check disk space
AVAILABLE_GB=$(df -BG "$SCRIPT_DIR" | awk 'NR==2 {print $4}' | sed 's/G//')
echo "Available disk space: ${AVAILABLE_GB}GB"

if [ "$AVAILABLE_GB" -lt 2 ]; then
  echo -e "${RED}WARNING: Less than 2GB disk space available${NC}"
  echo "Build may fail due to insufficient space"
  read -p "Continue anyway? (y/N) " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    exit 1
  fi
fi

# Check memory
TOTAL_MEM_GB=$(free -g | awk '/^Mem:/{print $2}')
echo "Total memory: ${TOTAL_MEM_GB}GB"

if [ "$TOTAL_MEM_GB" -lt 4 ]; then
  echo -e "${YELLOW}WARNING: Less than 4GB RAM. Build will be slow.${NC}"
fi

echo -e "${GREEN} System check passed${NC}"
echo

echo -e "${BLUE}=== Step 2: Installing Build Dependencies ===${NC}"

# Update package cache
echo "Updating package cache..."
apt-get update -qq

# Install essential build tools
echo "Installing build dependencies..."
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq \
  git \
  python3 \
  python3-pip \
  curl \
  wget \
  ninja-build \
  pkg-config \
  libnss3-dev \
  libglib2.0-dev \
  libgtk-3-dev \
  libatk1.0-dev \
  libatk-bridge2.0-dev \
  libcups2-dev \
  libxcomposite-dev \
  libxdamage-dev \
  libxrandr-dev \
  libgbm-dev \
  libpango1.0-dev \
  libcairo2-dev \
  libasound2-dev \
  libpulse-dev \
  libxtst-dev \
  libxss-dev \
  libudev-dev \
  libdrm-dev \
  2>&1 | grep -v "already the newest version" || true

echo -e "${GREEN} Build dependencies installed${NC}"
echo

echo -e "${BLUE}=== Step 3: Setting up depot_tools ===${NC}"

# Create build directory
mkdir -p "$BUILD_DIR"

# Check if depot_tools already exists
if [ -d "$DEPOT_TOOLS" ]; then
  echo "depot_tools found, updating..."
  cd "$DEPOT_TOOLS"
  git pull --quiet || true
else
  echo "Cloning depot_tools..."
  # Try official source first, fall back to GitHub mirror
  if ! git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS" 2>/dev/null; then
    echo "Official source failed, trying GitHub mirror..."
    git clone --depth=1 https://github.com/chromium/depot_tools.git "$DEPOT_TOOLS"
  fi
fi

echo -e "${GREEN} depot_tools ready${NC}"
echo

echo -e "${BLUE}=== Step 4: Configuring Environment ===${NC}"

# Add depot_tools to PATH
export PATH="${DEPOT_TOOLS}:${PATH}"

# Disable depot_tools auto-update (to save bandwidth/time)
export DEPOT_TOOLS_UPDATE=0

# Use system Python
export VPYTHON_BYPASS="manually managed python not supported by chrome operations"

echo "Environment configured:"
echo "  PATH includes: $DEPOT_TOOLS"
echo "  Python: $(python3 --version)"
echo "  Ninja: $(ninja --version)"

# Check if gn is available
if [ -f "${DEPOT_TOOLS}/gn" ]; then
  echo "  GN: Available in depot_tools"
elif command -v gn &> /dev/null; then
  echo "  GN: $(which gn)"
else
  echo -e "${YELLOW}  WARNING: gn not found, will be downloaded by gclient${NC}"
fi

echo -e "${GREEN} Environment configured${NC}"
echo

echo -e "${BLUE}=== Step 5: Verifying Chromium Source ===${NC}"

if [ ! -d "$CHROMIUM_SRC" ]; then
  echo -e "${RED}ERROR: Chromium source not found at $CHROMIUM_SRC${NC}"
  exit 1
fi

if [ ! -f "$CHROMIUM_SRC/.gn" ]; then
  echo -e "${RED}ERROR: .gn file not found, invalid Chromium source${NC}"
  exit 1
fi

echo "Chromium source verified at: $CHROMIUM_SRC"
echo -e "${GREEN} Source verified${NC}"
echo

echo -e "${BLUE}=== Step 6: Checking DataSipper Components ===${NC}"

# Verify our code exists
DATASIPPER_FILES=(
  "components/datasipper/datasipper_service.cc"
  "components/datasipper/streaming/stream_database_sync.cc"
  "components/datasipper/streaming/stream_registry.cc"
  "chrome/browser/ui/webui/side_panel/datasipper/datasipper_page_handler.cc"
)

cd "$CHROMIUM_SRC"
MISSING=0
for file in "${DATASIPPER_FILES[@]}"; do
  if [ ! -f "$file" ]; then
    echo -e "${RED} Missing: $file${NC}"
    MISSING=$((MISSING + 1))
  else
    echo -e "${GREEN} Found: $file${NC}"
  fi
done

if [ $MISSING -gt 0 ]; then
  echo -e "${RED}ERROR: $MISSING DataSipper files missing${NC}"
  exit 1
fi

echo -e "${GREEN} All DataSipper components present${NC}"
echo

echo -e "${BLUE}=== Step 7: Generating Build Configuration ===${NC}"

# Try to generate build files
cd "$CHROMIUM_SRC"

# Check if out/DataSipper already exists
if [ -d "out/DataSipper" ]; then
  echo "Build directory already exists, regenerating..."
fi

# Create GN args for a minimal build
echo "Creating build configuration..."
mkdir -p out/DataSipper

cat > out/DataSipper/args.gn << 'EOF'
# DataSipper minimal build configuration

# Build type
is_debug = true
is_component_build = true

# Disable features we don't need to save space/time
enable_nacl = false
enable_remoting = false
enable_google_apis = false
enable_chrome_extensions = false
enable_hangout_services_extension = false

# Use system libraries where possible
use_sysroot = false
use_custom_libcxx = false

# Minimal symbol info (saves space)
symbol_level = 1

# Speed up linking
use_lld = true

# Media codecs
proprietary_codecs = true
ffmpeg_branding = "Chrome"

# Target specific components only
target_os = "linux"
target_cpu = "x64"
EOF

echo -e "${GREEN} Build args created${NC}"
echo

# Now try to run gn gen
echo "Running: gn gen out/DataSipper"
if "${DEPOT_TOOLS}/gn" gen out/DataSipper 2>&1 | tee /tmp/gn_output.log; then
  echo -e "${GREEN} Build files generated successfully${NC}"
else
  echo -e "${RED}ERROR: gn gen failed${NC}"
  echo "See /tmp/gn_output.log for details"
  tail -50 /tmp/gn_output.log
  exit 1
fi

echo

echo -e "${GREEN}TPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPW${NC}"
echo -e "${GREEN}Q         Setup Complete!                        Q${NC}"
echo -e "${GREEN}ZPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP]${NC}"
echo
echo "Next steps:"
echo "  1. cd $CHROMIUM_SRC"
echo "  2. export PATH=\"${DEPOT_TOOLS}:\$PATH\""
echo "  3. ninja -C out/DataSipper components/datasipper"
echo
echo "To build just DataSipper components:"
echo "  ninja -C out/DataSipper components/datasipper:datasipper"
echo
echo "To see what can be built:"
echo "  gn ls out/DataSipper | grep datasipper"
echo

# Save environment for later use
cat > "${SCRIPT_DIR}/env.sh" << EOF
#!/bin/bash
# Source this file to set up environment
export PATH="${DEPOT_TOOLS}:\${PATH}"
export DEPOT_TOOLS_UPDATE=0
export VPYTHON_BYPASS="manually managed python not supported by chrome operations"
export CHROMIUM_SRC="${CHROMIUM_SRC}"
cd "${CHROMIUM_SRC}"
echo "Environment configured. Ready to build."
EOF

chmod +x "${SCRIPT_DIR}/env.sh"

echo -e "${BLUE}Environment saved to: ${SCRIPT_DIR}/env.sh${NC}"
echo "Run 'source env.sh' to restore environment in new shell"
echo
