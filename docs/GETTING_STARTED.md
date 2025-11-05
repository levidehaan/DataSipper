# DataSipper Development Getting Started Guide

This guide will help you set up a complete development environment for DataSipper and build your first custom Chromium browser with network monitoring capabilities.

## Prerequisites

### System Requirements
- **Operating System**: Arch Linux (other Linux distributions may work with modifications)
- **RAM**: Minimum 8GB, recommended 16GB+
- **Disk Space**: At least 100GB free space
- **Network**: Reliable internet connection for downloading Chromium source (~2GB)

### Required Knowledge
- Basic familiarity with C++ development
- Understanding of build systems (ninja, gn)
- Git version control
- Command line usage

## Quick Start

### 1. Clone DataSipper Repository
```bash
git clone <datasipper-repo-url>
cd datasipper
```

### 2. Install Dependencies
```bash
# Install all required Arch Linux packages
./scripts/install-deps-arch.sh
```

### 3. Set Up Environment
```bash
# Source the development environment
source scripts/setup-env.sh
```

### 4. Fetch Chromium Source
```bash
# Download and checkout specific Chromium version
./scripts/fetch-chromium.sh
```

### 5. Apply DataSipper Patches
```bash
# Apply all DataSipper modifications
./scripts/patches.py apply
```

### 6. Build DataSipper
```bash
cd chromium-src/src

# Configure build
gn gen out/DataSipper

# Build the browser
ninja -C out/DataSipper chrome
```

### 7. Run DataSipper
```bash
# Launch your custom browser
./out/DataSipper/chrome
```

## Detailed Setup Instructions

### Step 1: Environment Preparation

#### Install System Dependencies
The `install-deps-arch.sh` script will install all required packages:

```bash
./scripts/install-deps-arch.sh
```

This installs:
- Build tools (gcc, ninja, python, nodejs)
- Graphics libraries (gtk3, mesa, vulkan)
- Media libraries (ffmpeg, opus, libvpx)
- Network libraries (nss, openssl, libcups)
- Development tools (git, gdb, valgrind)

#### Verify Installation
```bash
# Check critical tools
which ninja
which python3
which node
which gn

# Verify system resources
free -h  # Check available RAM
df -h .  # Check available disk space
```

### Step 2: Chromium Source Setup

#### Understanding the Target Version
DataSipper targets Chromium 137.0.7151.x:
- Commit hash: `fb224f9793306dd9976b6e70901376a2c095a69e`
- Branch: `refs/branch-heads/7151`
- Stable release suitable for modifications

#### Source Download Process
```bash
# The fetch script will:
# 1. Clone depot_tools
# 2. Run 'fetch chromium'
# 3. Checkout specific commit
# 4. Run 'gclient sync'
./scripts/fetch-chromium.sh
```

**Note**: This process can take 30-60 minutes depending on your internet connection.

### Step 3: Understanding DataSipper Patches

#### Patch Categories
- **Core patches**: Essential DataSipper functionality
- **Extra patches**: Optional advanced features  
- **Upstream fixes**: Chromium bug fixes

#### Patch Application
```bash
# View all patches
./scripts/patches.py list

# Validate patches exist
./scripts/patches.py validate

# Apply in order
./scripts/patches.py apply
```

### Step 4: Build Configuration

#### Basic Build Setup
```bash
cd chromium-src/src

# Generate build configuration
gn gen out/DataSipper
```

#### Advanced Build Options
```bash
# Configure for development
gn gen out/DataSipper --args='
  is_debug=true
  enable_nacl=false
  enable_remoting=false
  use_cups=true
  proprietary_codecs=true
  ffmpeg_branding="Chrome"
'
```

#### Common Build Arguments
- `is_debug=true`: Debug build with symbols
- `is_component_build=true`: Faster incremental builds
- `symbol_level=1`: Reduced debug symbols for faster builds
- `enable_nacl=false`: Disable Native Client
- `use_jumbo_build=true`: Faster builds (may use more RAM)

### Step 5: Building DataSipper

#### Initial Build
```bash
# Full build (takes 1-3 hours first time)
ninja -C out/DataSipper chrome

# Monitor build progress
ninja -C out/DataSipper chrome | tee build.log
```

#### Incremental Builds
After making changes:
```bash
# Only rebuild what changed (much faster)
ninja -C out/DataSipper chrome
```

#### Parallel Building
```bash
# Use all CPU cores
ninja -C out/DataSipper -j$(nproc) chrome

# Limit parallel jobs (if running out of RAM)
ninja -C out/DataSipper -j4 chrome
```

### Step 6: Running and Testing

#### Basic Launch
```bash
cd chromium-src/src
./out/DataSipper/chrome
```

#### Development Launch
```bash
# Launch with debugging flags
./out/DataSipper/chrome \
  --enable-logging=stderr \
  --log-level=0 \
  --disable-web-security \
  --user-data-dir=/tmp/datasipper-dev
```

#### Testing DataSipper Features
1. Open the browser
2. Look for the DataSipper panel toggle button
3. Navigate to a website with API calls
4. Open the DataSipper panel to see captured network traffic

## Development Workflow

### Making Changes

#### 1. Create a New Feature Branch
```bash
cd chromium-src/src
git checkout -b feature/my-new-feature
```

#### 2. Develop Using Patches
```bash
# Source quilt environment
source ../../scripts/set_quilt_vars.sh

# Create new patch
qnew core/datasipper/my-new-feature.patch

# Add files to patch
qadd path/to/file.cc
qadd path/to/file.h

# Make changes
qedit path/to/file.cc

# Update patch
qrefresh
```

#### 3. Test Changes
```bash
# Build with changes
ninja -C out/DataSipper chrome

# Test functionality
./out/DataSipper/chrome
```

#### 4. Update Patch Series
Edit `patches/series` to include new patch:
```
core/datasipper/my-new-feature.patch
```

### Debugging

#### Build Issues
```bash
# Clean build
rm -rf out/DataSipper
gn gen out/DataSipper
ninja -C out/DataSipper chrome

# Check build arguments
gn args out/DataSipper --list
```

#### Runtime Issues
```bash
# Launch with debug output
./out/DataSipper/chrome --enable-logging=stderr --log-level=0

# Use debugger
gdb ./out/DataSipper/chrome
```

#### Patch Issues
```bash
# Remove all patches
./scripts/patches.py reverse

# Apply patches one by one
cd chromium-src/src
source ../../scripts/set_quilt_vars.sh
qpush  # Apply next patch, investigate if it fails
```

## Common Issues and Solutions

### Build Failures

#### Out of Memory
```bash
# Reduce parallel jobs
ninja -C out/DataSipper -j2 chrome

# Use component build
gn gen out/DataSipper --args='is_component_build=true'
```

#### Missing Dependencies
```bash
# Reinstall dependencies
./scripts/install-deps-arch.sh

# Update depot_tools
cd build/depot_tools
git pull
```

### Patch Failures

#### Patch Doesn't Apply
```bash
# Check if files have moved
find chromium-src/src -name "target_file.cc"

# Update patch paths if needed
vim patches/core/datasipper/problematic.patch
```

#### Merge Conflicts
```bash
# Force apply and fix manually
qpush -f
qedit conflicted_file.cc
qrefresh
```

### Runtime Issues

#### DataSipper Panel Not Visible
1. Check build included UI patches
2. Verify JavaScript resources are built
3. Check browser console for errors

#### Network Interception Not Working
1. Verify network patches applied correctly
2. Check debug output for interception logs
3. Test with simple HTTP requests first

## Next Steps

### Explore the Codebase
- `chromium-src/src/`: Main Chromium source
- `patches/core/`: Essential DataSipper modifications
- `docs/`: Additional documentation

### Learn Patch Development
- Read `docs/PATCH_DEVELOPMENT.md`
- Practice with small changes
- Understand quilt workflow

### Contribute
- Follow coding standards
- Test thoroughly
- Document changes
- Submit patches for review

### Advanced Topics
- Custom build configurations
- Performance optimization
- Cross-platform builds
- Automated testing

## Getting Help

### Documentation
- Chromium Developer Documentation: https://www.chromium.org/developers/
- GN Reference: https://gn.googlesource.com/gn/
- Ninja Manual: https://ninja-build.org/manual.html

### Community
- DataSipper Issues: Create issues for bugs or questions
- Chromium Development: https://groups.google.com/a/chromium.org/g/chromium-dev

### Debugging Resources
- Chromium Debugging: https://www.chromium.org/developers/how-tos/debugging/
- GDB with Chromium: https://www.chromium.org/developers/debugging-with-gdb/

Remember: Building Chromium is a complex process. Don't get discouraged if you encounter issues – they're normal and solvable with patience and the right approach!