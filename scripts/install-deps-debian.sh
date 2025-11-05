#!/bin/bash

# DataSipper Dependencies Installation Script for Debian/Ubuntu
# Based on the comprehensive plan and Chromium build requirements

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_info "DataSipper Dependencies Installation for Debian/Ubuntu"
log_info "======================================================"

# Check if running as root or with sudo
if [[ $EUID -eq 0 ]]; then
    SUDO=""
else
    SUDO="sudo"
    log_info "Running with sudo for package installation"
fi

# Check Ubuntu/Debian version
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    log_info "Detected: $PRETTY_NAME"
else
    log_error "Cannot detect OS version"
    exit 1
fi

# Update package lists
log_info "Updating package lists..."
$SUDO apt update

# Install essential build tools
log_info "Installing essential build tools..."
$SUDO apt install -y \
    build-essential \
    git \
    curl \
    wget \
    unzip \
    zip \
    python3 \
    python3-pip \
    python3-setuptools \
    python3-dev \
    nodejs \
    npm \
    ninja-build \
    pkg-config \
    libtool \
    autoconf \
    automake \
    cmake \
    gperf \
    bison \
    flex \
    yasm \
    nasm

# Install Chromium-specific dependencies
log_info "Installing Chromium build dependencies..."

# Development libraries for UI and graphics
$SUDO apt install -y \
    libgtk-3-dev \
    libgtk-4-dev \
    libx11-dev \
    libxext-dev \
    libxfixes-dev \
    libxdamage-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxi-dev \
    libxrandr-dev \
    libxrender-dev \
    libxss-dev \
    libxtst-dev \
    libxkbcommon-dev \
    libdrm-dev \
    libva-dev \
    libvdpau-dev

# Graphics and rendering libraries  
$SUDO apt install -y \
    mesa-common-dev \
    libgl1-mesa-dev \
    libgles2-mesa-dev \
    libegl1-mesa-dev \
    libgbm-dev \
    vulkan-headers \
    vulkan-validationlayers-dev

# Audio libraries
$SUDO apt install -y \
    libasound2-dev \
    libpulse-dev \
    libpulse0 \
    pulseaudio-utils

# Font and text rendering
$SUDO apt install -y \
    libfontconfig1-dev \
    libfreetype6-dev \
    libharfbuzz-dev \
    libpango1.0-dev \
    libcairo2-dev

# Media libraries
$SUDO apt install -y \
    libjpeg-dev \
    libpng-dev \
    libwebp-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libopus-dev \
    libvpx-dev \
    libvorbis-dev \
    libflac-dev \
    libspeex-dev

# Network and security libraries
$SUDO apt install -y \
    libnss3-dev \
    libnspr4-dev \
    libssl-dev \
    libkrb5-dev \
    libcups2-dev

# System libraries
$SUDO apt install -y \
    libdbus-1-dev \
    libglib2.0-dev \
    libc6-dev \
    zlib1g-dev \
    libbz2-dev \
    liblzma-dev \
    libffi-dev \
    libexpat1-dev \
    libxml2-dev \
    libxslt1-dev \
    libsystemd-dev

# Additional utilities
$SUDO apt install -y \
    p7zip-full \
    subversion \
    valgrind \
    ccache \
    gdb

# Install DataSipper-specific dependencies
log_info "Installing DataSipper-specific dependencies..."

# Kafka client library (librdkafka)
$SUDO apt install -y \
    librdkafka-dev \
    librdkafka1

# Redis client library (hiredis)
$SUDO apt install -y \
    libhiredis-dev \
    libhiredis0.14

# MySQL client library
$SUDO apt install -y \
    libmysqlclient-dev \
    default-libmysqlclient-dev

# Python dependencies for build scripts
log_info "Installing Python dependencies..."
pip3 install --user \
    requests \
    setuptools \
    protobuf

# Install depot_tools if not already present
DEPOT_TOOLS_DIR="$(dirname "$0")/../build/depot_tools"
if [[ ! -d "$DEPOT_TOOLS_DIR" ]]; then
    log_info "Installing depot_tools..."
    mkdir -p "$(dirname "$DEPOT_TOOLS_DIR")"
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
else
    log_info "depot_tools already exists, updating..."
    cd "$DEPOT_TOOLS_DIR"
    git pull
fi

# Install Node.js dependencies if needed
log_info "Setting up Node.js environment..."
npm install -g yarn

# Check for optional ccache setup
if command -v ccache >/dev/null 2>&1; then
    log_info "Setting up ccache for faster builds..."
    ccache --max-size=50G
    ccache --set-config=compression=true
    export CC="ccache gcc"
    export CXX="ccache g++"
fi

# Verify critical tools
log_info "Verifying installation..."

# Check for essential tools
REQUIRED_TOOLS=(
    "git"
    "python3" 
    "ninja"
    "pkg-config"
    "nodejs"
    "npm"
)

MISSING_TOOLS=()
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        MISSING_TOOLS+=("$tool")
    fi
done

if [[ ${#MISSING_TOOLS[@]} -eq 0 ]]; then
    log_info "✓ All essential tools are available"
else
    log_error "✗ Missing tools: ${MISSING_TOOLS[*]}"
    exit 1
fi

# Check for critical libraries
log_info "Checking critical libraries..."
REQUIRED_LIBS=(
    "libgtk-3-dev"
    "libnss3-dev" 
    "libx11-dev"
    "librdkafka-dev"
    "libhiredis-dev"
)

MISSING_LIBS=()
for lib in "${REQUIRED_LIBS[@]}"; do
    if ! dpkg -s "$lib" >/dev/null 2>&1; then
        MISSING_LIBS+=("$lib")
    fi
done

if [[ ${#MISSING_LIBS[@]} -eq 0 ]]; then
    log_info "✓ All critical libraries are installed"
else
    log_error "✗ Missing libraries: ${MISSING_LIBS[*]}"
    exit 1
fi

# Display environment information
log_info "Installation Summary:"
echo "  OS: $PRETTY_NAME"
echo "  Python: $(python3 --version 2>&1)"
echo "  Node.js: $(node --version 2>&1)"
echo "  Git: $(git --version 2>&1)"
echo "  Ninja: $(ninja --version 2>&1)"

if command -v ccache >/dev/null 2>&1; then
    echo "  ccache: $(ccache --version | head -1)"
fi

log_info "Depot tools path: $DEPOT_TOOLS_DIR"

echo ""
log_info "✓ DataSipper dependencies installation completed successfully!"
echo ""
log_info "Next steps:"
echo "  1. Source the environment: source scripts/setup-env.sh"
echo "  2. Fetch Chromium source: ./scripts/fetch-chromium.sh"
echo "  3. Configure build: ./scripts/configure-build.sh dev"
echo "  4. Build DataSipper: ninja -C chromium-src/src/out/DataSipper chrome"
echo ""

# Set up environment variables for this session
export PATH="$DEPOT_TOOLS_DIR:$PATH"
export DEPOT_TOOLS_UPDATE=1
export DEPOT_TOOLS_METRICS=0

log_info "Environment configured for this session."
log_info "Add the following to your ~/.bashrc for permanent setup:"
echo "export PATH=\"$DEPOT_TOOLS_DIR:\$PATH\""
echo "export DEPOT_TOOLS_UPDATE=1"
echo "export DEPOT_TOOLS_METRICS=0"