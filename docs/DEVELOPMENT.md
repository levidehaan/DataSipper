# DataSipper Development Guide

This guide covers setting up the development environment for DataSipper, a network data interception tool built on Chromium.

## Table of Contents

- [Quick Start](#quick-start)
- [Local Development Setup](#local-development-setup)
- [Docker Development](#docker-development)
- [Build Configurations](#build-configurations)
- [Development Workflow](#development-workflow)
- [Debugging](#debugging)
- [Contributing](#contributing)

## Quick Start

### Prerequisites

- **Arch Linux** (currently required for build scripts)
- **100GB+ free disk space**
- **8GB+ RAM** (16GB+ recommended)
- **Fast internet connection** (Chromium source is ~10GB)

### One-Command Setup

```bash
# Clone the repository
git clone https://github.com/your-org/datasipper.git
cd datasipper

# Run complete setup (will take 1-4 hours)
./scripts/dev-setup.sh
```

This script will:
1. ✅ Check system requirements
2. 📦 Install all dependencies
3. 🔧 Setup development environment
4. 📥 Fetch Chromium source (~30-60 minutes)
5. 🩹 Apply DataSipper patches
6. ⚙️ Configure build system
7. 🏗️ Build DataSipper (optional, 1-4 hours)

## Local Development Setup

### Manual Step-by-Step Setup

If you prefer manual control or need to troubleshoot:

```bash
# 1. Install dependencies
./scripts/install-deps-arch.sh

# 2. Setup environment
source ./scripts/setup-env.sh

# 3. Fetch Chromium source
./scripts/fetch-chromium.sh

# 4. Apply DataSipper patches
cd chromium-src/src
python3 ../../scripts/patches.py apply

# 5. Configure build
cd ../..
./scripts/configure-build.sh dev

# 6. Build DataSipper
ninja -C chromium-src/src/out/DataSipper chrome
```

### Build Types

Choose your build type based on your needs:

| Build Type | Use Case | Build Time | Size | Debug Info |
|------------|----------|------------|------|------------|
| `dev` | Development (default) | 1-2 hours | ~12GB | Minimal |
| `debug` | Deep debugging | 2-4 hours | ~15GB | Full |
| `release` | Production/testing | 1-3 hours | ~8GB | None |

```bash
# Configure specific build type
./scripts/configure-build.sh debug   # For debugging
./scripts/configure-build.sh release # For production
```

### Environment Setup

The development environment requires several environment variables:

```bash
# Source the environment (add to ~/.bashrc for persistence)
source scripts/setup-env.sh

# Or set manually:
export PATH="/path/to/datasipper/build/depot_tools:$PATH"
export DEPOT_TOOLS_UPDATE=1
export DEPOT_TOOLS_METRICS=0
export GYP_DEFINES="target_arch=x64"
```

## Docker Development

For containerized development (useful for CI or isolated environments):

### Development Container

```bash
# Build development container
docker build -f Dockerfile.dev --target development -t datasipper:dev .

# Run development container with source mounting
docker run -it --rm \
  -v $(pwd):/home/datasipper/datasipper \
  -v datasipper-chromium:/home/datasipper/datasipper/chromium-src \
  -v datasipper-cache:/home/datasipper/datasipper/build-cache \
  datasipper:dev

# Inside container, run setup
./scripts/dev-setup.sh --skip-build
```

### Production Build Container

```bash
# Build production container (takes several hours)
docker build -f Dockerfile.dev --target runtime -t datasipper:latest .

# Run DataSipper
docker run -it --rm \
  -p 8080:8080 \
  datasipper:latest
```

## Build Configurations

### Common Build Commands

```bash
# Full clean build
rm -rf chromium-src/src/out && ./scripts/configure-build.sh dev
ninja -C chromium-src/src/out/DataSipper chrome

# Incremental build (after code changes)
ninja -C chromium-src/src/out/DataSipper chrome

# Parallel build (faster on multi-core systems)
ninja -C chromium-src/src/out/DataSipper -j$(nproc) chrome

# Limited parallel build (if memory constrained)
ninja -C chromium-src/src/out/DataSipper -j4 chrome

# Build specific targets
ninja -C chromium-src/src/out/DataSipper content_browsertests  # Browser tests
ninja -C chromium-src/src/out/DataSipper unit_tests            # Unit tests
```

### Build Customization

Edit `chromium-src/src/out/DataSipper/args.gn` to customize build:

```gn
# Enable/disable DataSipper features
datasipper_enabled = true
datasipper_network_interception = true
datasipper_ui_panel = true
datasipper_external_integrations = true

# Performance tuning
use_jumbo_build = true      # Faster builds
use_thin_lto = false        # Disable for faster builds
symbol_level = 1            # Balanced debug info

# Development features
is_component_build = true   # Faster incremental builds
enable_iterator_debugging = true  # Extra debugging (debug builds)
```

## Development Workflow

### 1. Patch Development

DataSipper uses a patch-based approach to modify Chromium:

```bash
# View available patches
ls patches/core/

# Apply all patches
python3 scripts/patches.py apply

# Remove all patches
python3 scripts/patches.py remove

# Apply specific patch series
python3 scripts/patches.py apply --series core

# Create new patch
cd chromium-src/src
# Make your changes...
git add .
git commit -m "Add new feature"
git format-patch -1 --stdout > ../../patches/core/new-feature.patch
```

### 2. Testing Changes

```bash
# Build and test
ninja -C chromium-src/src/out/DataSipper chrome
./chromium-src/src/out/DataSipper/chrome

# Run with debugging
./chromium-src/src/out/DataSipper/chrome --enable-logging --v=1

# Run tests
ninja -C chromium-src/src/out/DataSipper browser_tests
./chromium-src/src/out/DataSipper/browser_tests --gtest_filter="*DataSipper*"
```

### 3. Debugging

```bash
# Debug build for GDB
./scripts/configure-build.sh debug
ninja -C chromium-src/src/out/Debug chrome

# Run with GDB
gdb ./chromium-src/src/out/Debug/chrome
(gdb) run --no-sandbox --disable-dev-shm-usage

# Enable verbose logging
./chromium-src/src/out/Debug/chrome --enable-logging --v=2 --log-level=0

# Chrome DevTools debugging
./chromium-src/src/out/Debug/chrome --remote-debugging-port=9222
```

### 4. Code Style

Follow Chromium's coding standards:

```bash
# Format code
git clang-format  # Staged changes only
git clang-format HEAD~1  # Format last commit

# Run linter
vpython3 tools/checkdeps/checkdeps.py

# Check style
vpython3 tools/clang/scripts/run_tool.py clang-tidy
```

## Project Structure

```
datasipper/
├── chromium-src/           # Chromium source (fetched)
│   └── src/
├── patches/                # DataSipper patches
│   ├── core/              # Core functionality patches
│   │   ├── datasipper/    # Base infrastructure
│   │   ├── network-interception/  # Network hooks
│   │   └── ui-panel/      # User interface
│   └── extra/             # Optional features
├── scripts/               # Build and setup scripts
├── docs/                  # Documentation
├── build/                 # Build artifacts
│   └── depot_tools/       # Chromium build tools
└── README.md
```

## Common Issues

### Build Failures

```bash
# Out of memory during build
ninja -C chromium-src/src/out/DataSipper -j2 chrome  # Reduce parallelism

# Disk space issues
df -h  # Check available space
du -sh chromium-src/  # Check Chromium size

# Corrupted build
rm -rf chromium-src/src/out && ./scripts/configure-build.sh dev
```

### Patch Issues

```bash
# Patch conflicts
python3 scripts/patches.py remove  # Remove all patches
cd chromium-src/src && git reset --hard HEAD  # Reset changes
python3 ../../scripts/patches.py apply  # Reapply patches

# Update Chromium
cd chromium-src/src
git fetch origin
git checkout NEW_COMMIT_HASH
gclient sync
```

### Runtime Issues

```bash
# Sandbox issues (development only)
./chrome --no-sandbox --disable-dev-shm-usage

# Display issues in containers
xhost +local:docker  # Allow Docker X11 access

# Missing libraries
ldd ./chrome | grep "not found"  # Check missing dependencies
```

## Performance Tips

### Build Performance

- **Use SSD storage** for source and build directories
- **Increase RAM** - 16GB+ recommended for parallel builds
- **Use ccache** - `export USE_CCACHE=1` before building
- **Limit parallel jobs** - `ninja -j4` instead of `ninja -j$(nproc)`

### Development Performance

- **Component builds** - Faster incremental builds
- **Jumbo builds** - Reduces compile time
- **Shared libraries** - `is_component_build=true`

```gn
# Performance-optimized development build
is_debug = false
is_component_build = true
symbol_level = 1
use_jumbo_build = true
enable_nacl = false
```

## Contributing

### Before Contributing

1. Set up development environment
2. Read [Chromium contributing guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/contributing.md)
3. Understand patch-based development workflow
4. Test your changes thoroughly

### Submitting Changes

1. Create feature branch: `git checkout -b feature/your-feature`
2. Make changes and create patches
3. Test changes: build and run DataSipper
4. Submit pull request with:
   - Clear description of changes
   - Test instructions
   - Updated documentation if needed

### Patch Guidelines

- Keep patches focused and atomic
- Include clear commit messages
- Test patches on clean Chromium checkout
- Document new features in comments
- Follow Chromium security guidelines

## Additional Resources

- [Chromium Development](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md)
- [GN Build Configuration](https://gn.googlesource.com/gn/+/main/docs/reference.md)
- [Chromium Debugging](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/debugging.md)
- [DataSipper Documentation](./GETTING_STARTED.md)

---

Happy coding! 🚀 If you encounter issues, please check the [troubleshooting section](#common-issues) or open an issue.