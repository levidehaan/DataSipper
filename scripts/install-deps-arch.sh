#!/bin/bash

# DataSipper Arch Linux Dependencies Installation Script
# This script installs all required dependencies for building Chromium on Arch Linux

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}DataSipper Arch Linux Dependencies Installation${NC}"
echo "=============================================="

# Check if running on Arch Linux
if ! command -v pacman &> /dev/null; then
    echo -e "${RED}✗ This script is designed for Arch Linux systems with pacman${NC}"
    exit 1
fi

echo -e "${BLUE}Installing core build dependencies...${NC}"

# Core build system dependencies (excluding python/nodejs - using uv/nvm)
CORE_DEPS=(
    base-devel
    git
    ninja
)

# Graphics and GUI libraries
GRAPHICS_DEPS=(
    gtk3
    gtk4
    libx11
    libxext
    libxfixes
    libxdamage
    libxcomposite
    libxcursor
    libxi
    libxrandr
    libxrender
    libxss
    libxtst
    mesa
    libdrm
    libva
    libvdpau
    vulkan-headers
    vulkan-validation-layers
)

# Audio libraries
AUDIO_DEPS=(
    alsa-lib
    pulseaudio
    libpulse
)

# Font and text rendering
FONT_DEPS=(
    fontconfig
    freetype2
    harfbuzz
    pango
    cairo
)

# Image and media libraries
MEDIA_DEPS=(
    libjpeg-turbo
    libpng
    libwebp
    ffmpeg
    opus
    libvpx
    libvorbis
    flac
    speex
)

# Networking and security
NETWORK_DEPS=(
    nss
    nspr
    openssl
    krb5
    libcups
)

# System libraries
SYSTEM_DEPS=(
    dbus
    glib2
    glibc
    gcc-libs
    zlib
    bzip2
    xz
    libffi
    expat
    libxml2
    libxslt
    systemd-libs
)

# Additional dependencies
ADDITIONAL_DEPS=(
    p7zip
    unzip
    zip
    subversion
    wget
    curl
    re2
    snappy
    minizip
    libevent
    libusb
    jdk-openjdk
    gdb
    valgrind
    clang
    llvm
)

# Combine all dependencies
ALL_DEPS=(
    "${CORE_DEPS[@]}"
    "${GRAPHICS_DEPS[@]}"
    "${AUDIO_DEPS[@]}"
    "${FONT_DEPS[@]}"
    "${MEDIA_DEPS[@]}"
    "${NETWORK_DEPS[@]}"
    "${SYSTEM_DEPS[@]}"
    "${ADDITIONAL_DEPS[@]}"
)

echo -e "${YELLOW}The following packages will be installed:${NC}"
printf '%s\n' "${ALL_DEPS[@]}" | column -c 80

echo ""
read -p "Continue with installation? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}Installation cancelled.${NC}"
    exit 1
fi

echo -e "${BLUE}Installing packages with pacman...${NC}"
sudo pacman -S --needed "${ALL_DEPS[@]}"

echo -e "${GREEN}✓ All official repository packages installed${NC}"

# Set up Python environment with uv
echo -e "${BLUE}Setting up Python environment with uv...${NC}"
if ! command -v uv &> /dev/null; then
    echo -e "${RED}✗ uv not found. Please install uv first: curl -LsSf https://astral.sh/uv/install.sh | sh${NC}"
    exit 1
fi

# Create virtual environment if it doesn't exist
if [ ! -d ".venv" ]; then
    echo "Creating Python virtual environment..."
    uv venv
fi

# Activate virtual environment and install Python dependencies
echo "Installing Python build dependencies..."
source .venv/bin/activate
uv pip install setuptools wheel

echo -e "${GREEN}✓ Python environment set up with uv${NC}"

# Set up Node.js environment with nvm
echo -e "${BLUE}Setting up Node.js environment with nvm...${NC}"
if [ ! -d "$HOME/.nvm" ]; then
    echo -e "${RED}✗ nvm not found. Please install nvm first${NC}"
    exit 1
fi

# Source nvm
export NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"

# Install and use Node.js LTS
echo "Installing/using Node.js LTS..."
nvm install --lts
nvm use --lts

echo -e "${GREEN}✓ Node.js environment set up with nvm${NC}"

# Check for AUR helper
AUR_HELPER=""
if command -v yay &> /dev/null; then
    AUR_HELPER="yay"
elif command -v paru &> /dev/null; then
    AUR_HELPER="paru"
elif command -v pikaur &> /dev/null; then
    AUR_HELPER="pikaur"
fi

if [ -n "$AUR_HELPER" ]; then
    echo -e "${BLUE}Installing AUR packages with ${AUR_HELPER}...${NC}"
    $AUR_HELPER -S --needed depot-tools-git
    echo -e "${GREEN}✓ AUR packages installed${NC}"
else
    echo -e "${YELLOW}⚠ No AUR helper found (yay, paru, pikaur)${NC}"
    echo "You will need to manually install:"
    echo "  - depot-tools-git (from AUR)"
    echo ""
    echo "Or install depot_tools manually by cloning:"
    echo "  git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git"
fi

echo ""
echo -e "${GREEN}✓ Dependency installation completed${NC}"
echo ""
echo -e "${YELLOW}System Requirements Check:${NC}"

# Check available memory
TOTAL_MEM=$(free -g | awk '/^Mem:/{print $2}')
if [ "$TOTAL_MEM" -lt 8 ]; then
    echo -e "${RED}⚠ Warning: Less than 8GB RAM detected (${TOTAL_MEM}GB)${NC}"
    echo "Building Chromium requires at least 8GB RAM, preferably 16GB+"
else
    echo -e "${GREEN}✓ Sufficient RAM detected (${TOTAL_MEM}GB)${NC}"
fi

# Check available disk space
AVAILABLE_SPACE=$(df -BG . | awk 'NR==2{print $4}' | sed 's/G//')
if [ "$AVAILABLE_SPACE" -lt 100 ]; then
    echo -e "${RED}⚠ Warning: Less than 100GB free space detected (${AVAILABLE_SPACE}GB)${NC}"
    echo "Building Chromium requires at least 100GB free disk space"
else
    echo -e "${GREEN}✓ Sufficient disk space available (${AVAILABLE_SPACE}GB)${NC}"
fi

echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Activate Python environment: source .venv/bin/activate"
echo "2. Activate Node.js environment: nvm use --lts"
echo "3. Source the environment: source scripts/setup-env.sh"
echo "4. Configure build: cd chromium-src/src && gn gen out/Default"
echo "5. Build Chromium: ninja -C out/Default chrome"
echo ""
echo -e "${BLUE}Note: Chromium source has already been fetched to chromium-src/src${NC}"