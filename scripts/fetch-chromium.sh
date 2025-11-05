#!/bin/bash

# DataSipper Chromium Source Fetching Script
# This script fetches the specific Chromium version for DataSipper development

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}DataSipper Chromium Source Fetching${NC}"
echo "==================================="

# Get the script directory (project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
CHROMIUM_DIR="${PROJECT_ROOT}/chromium-src"

# Target commit hash for refs/branch-heads/7151
TARGET_COMMIT="6d0796400dc7f4912cf196e27314fd51731de2d2"

echo -e "${YELLOW}Project root: ${PROJECT_ROOT}${NC}"
echo -e "${YELLOW}Chromium directory: ${CHROMIUM_DIR}${NC}"
echo -e "${YELLOW}Target commit: ${TARGET_COMMIT}${NC}"

# Source the environment setup (safely)
if ! source "${PROJECT_ROOT}/scripts/setup-env.sh"; then
    echo -e "${RED}✗ Failed to setup environment${NC}"
    exit 1
fi

# Create chromium source directory
mkdir -p "$CHROMIUM_DIR"
cd "$CHROMIUM_DIR"

echo -e "${BLUE}Fetching Chromium source...${NC}"

if [ ! -d "src" ]; then
    echo -e "${YELLOW}Initial fetch of Chromium source (this will take a while)...${NC}"
    fetch chromium
else
    echo -e "${YELLOW}Chromium source already exists, updating...${NC}"
fi

cd src

echo -e "${BLUE}Checking out specific commit: ${TARGET_COMMIT}${NC}"

# Fetch the specific branch and commit
git fetch origin refs/branch-heads/7151:refs/remotes/origin/branch-heads/7151
git checkout "$TARGET_COMMIT"

echo -e "${BLUE}Syncing dependencies...${NC}"
gclient sync --with_branch_heads --with_tags

# Verify the checkout
CURRENT_COMMIT=$(git rev-parse HEAD)
if [ "$CURRENT_COMMIT" = "$TARGET_COMMIT" ]; then
    echo -e "${GREEN}✓ Successfully checked out commit: ${CURRENT_COMMIT}${NC}"
else
    echo -e "${RED}✗ Failed to checkout target commit${NC}"
    echo "Expected: $TARGET_COMMIT"
    echo "Got: $CURRENT_COMMIT"
    exit 1
fi

echo -e "${GREEN}✓ Chromium source fetch completed successfully${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Install Arch Linux dependencies: sudo pacman -S base-devel git python nodejs npm ninja gtk3 ..."
echo "2. Configure build: cd ${CHROMIUM_DIR}/src && gn gen out/Default"
echo "3. Build Chromium: ninja -C out/Default chrome"
