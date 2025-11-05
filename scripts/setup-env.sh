#!/bin/bash

# DataSipper Development Environment Setup Script
# This script sets up the environment for building Chromium and DataSipper

# Remove set -e to prevent terminal closure when sourced

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}DataSipper Development Environment Setup${NC}"
echo "========================================"

# Get the script directory (project root)
# Handle both sourced and executed cases
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Script is being executed
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    # Script is being sourced
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Fallback: if we're in the datasipper directory, use current directory
if [[ "$(basename "$PWD")" == "datasipper" ]]; then
    PROJECT_ROOT="$PWD"
fi

echo -e "${YELLOW}Project root: ${PROJECT_ROOT}${NC}"

# Add depot_tools to PATH
export PATH="${PROJECT_ROOT}/build/depot_tools:$PATH"

# Verify depot_tools installation
if command -v gclient &> /dev/null; then
    echo -e "${GREEN}✓ depot_tools found and accessible${NC}"
else
    echo -e "${RED}✗ depot_tools not found. Please run this script from the project root.${NC}"
    return 1 2>/dev/null || exit 1  # Use return when sourced, exit when executed
fi

# Set up Chromium-specific environment variables
export DEPOT_TOOLS_UPDATE=1
export DEPOT_TOOLS_METRICS=0  # Disable metrics collection

# Chromium build configuration
export GYP_DEFINES="target_arch=x64"
export CHROMIUM_BUILDTOOLS_PATH="${PROJECT_ROOT}/chromium-src/src/buildtools"

echo -e "${GREEN}Environment variables set:${NC}"
echo "PATH includes: ${PROJECT_ROOT}/build/depot_tools"
echo "DEPOT_TOOLS_UPDATE=1"
echo "DEPOT_TOOLS_METRICS=0"
echo "GYP_DEFINES=target_arch=x64"

echo ""
echo -e "${YELLOW}To use this environment in your current shell, run:${NC}"
echo "source ${PROJECT_ROOT}/scripts/setup-env.sh"
echo ""
echo -e "${YELLOW}Or add this to your ~/.bashrc or ~/.zshrc:${NC}"
echo "export PATH=\"${PROJECT_ROOT}/build/depot_tools:\$PATH\""