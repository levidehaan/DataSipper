#!/bin/bash

# DataSipper Docker Development Entrypoint
# Sets up the development environment inside container

set -e

echo "DataSipper Development Container"
echo "==============================="

# Source DataSipper environment
if [ -f "/home/datasipper/datasipper/scripts/setup-env.sh" ]; then
    source /home/datasipper/datasipper/scripts/setup-env.sh
fi

# Change to project directory
cd /home/datasipper/datasipper

# Display helpful information
echo ""
echo "Available Commands:"
echo "  ./scripts/dev-setup.sh     - Complete setup (if not done)"
echo "  ./scripts/fetch-chromium.sh - Fetch Chromium source"
echo "  ./scripts/configure-build.sh - Configure build"
echo "  ninja -C chromium-src/src/out/DataSipper chrome - Build DataSipper"
echo ""

echo "Development Directories:"
echo "  /home/datasipper/datasipper/          - Project root"
echo "  /home/datasipper/datasipper/patches/  - DataSipper patches"
echo "  /home/datasipper/datasipper/scripts/  - Build scripts"
echo ""

echo "To build DataSipper:"
echo "  1. Run: ./scripts/dev-setup.sh"
echo "  2. Or manually: ./scripts/fetch-chromium.sh && ./scripts/configure-build.sh dev"
echo "  3. Build: ninja -C chromium-src/src/out/DataSipper chrome"
echo ""

# Execute the provided command or start bash
exec "$@"