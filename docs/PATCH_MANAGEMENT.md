# DataSipper Patch Management Guide

This guide covers the comprehensive patch management system for DataSipper development.

## Overview

DataSipper uses a patch-based approach to modify Chromium, similar to Ungoogled Chromium. This allows for:
- **Clean separation** of DataSipper modifications from upstream Chromium
- **Easy upstream updates** by reapplying patches to new Chromium versions
- **Modular development** with patches organized by functionality
- **Version control** of all modifications

## Patch Organization

Patches are organized in a hierarchical structure under `patches/`:

```
patches/
├── series                    # Defines patch application order
├── upstream-fixes/          # Compatibility and safety patches
│   ├── build-system-fixes.patch
│   └── network-stack-compatibility.patch
├── core/                    # Essential DataSipper functionality
│   ├── datasipper/         # Base infrastructure
│   ├── network-interception/  # HTTP/WebSocket capture
│   └── ui-panel/           # User interface
└── extra/                   # Optional features
    ├── external-integrations/
    └── ui-panel/
```

### Patch Categories

| Category | Purpose | Dependency Level |
|----------|---------|-----------------|
| `upstream-fixes/` | Chromium compatibility fixes | Required |
| `core/datasipper/` | Base infrastructure | Required |
| `core/network-interception/` | Network capture | Required |
| `core/ui-panel/` | Basic UI | Recommended |
| `extra/` | Advanced features | Optional |

## Patch Management Commands

### Status and Information

```bash
# Show comprehensive system status
python3 scripts/patches.py status

# List all patches with existence status
python3 scripts/patches.py list

# Validate all patches exist
python3 scripts/patches.py validate
```

### Applying Patches

```bash
# Apply all patches
python3 scripts/patches.py apply

# Dry run (test without applying)
python3 scripts/patches.py apply --dry-run

# Apply only core patches
python3 scripts/patches.py apply --series core

# Apply specific patch series
python3 scripts/patches.py apply --series upstream-fixes
python3 scripts/patches.py apply --series core/datasipper
python3 scripts/patches.py apply --series extra

# Force apply (continue on errors)
python3 scripts/patches.py apply --force

# Apply without creating git commit
python3 scripts/patches.py apply --no-commit
```

### Removing Patches

```bash
# Remove all applied patches (reverse order)
python3 scripts/patches.py reverse
```

### Creating New Patches

```bash
# Create new patch in default category
python3 scripts/patches.py create my-feature

# Create patch in specific category
python3 scripts/patches.py create websocket-enhancement --category core/network-interception
python3 scripts/patches.py create kafka-improvements --category extra/external-integrations
```

## Development Workflow

### 1. Initial Setup

```bash
# Fetch Chromium source
./scripts/fetch-chromium.sh

# Check patch system status
python3 scripts/patches.py status

# Apply all patches
python3 scripts/patches.py apply
```

### 2. Making Changes

```bash
# Navigate to Chromium source
cd chromium-src/src

# Create feature branch
git checkout -b feature/my-datasipper-feature

# Make your changes to Chromium files
# Edit files, add functionality, etc.

# Test your changes
cd ../..
./scripts/configure-build.sh dev
ninja -C chromium-src/src/out/DataSipper chrome
```

### 3. Creating Patches

```bash
# Navigate back to Chromium source
cd chromium-src/src

# Add your changes
git add .
git commit -m "Add new DataSipper feature"

# Generate patch
git format-patch -1 --stdout > ../../patches/core/datasipper/my-feature.patch

# Add to series file (maintain proper order)
echo "core/datasipper/my-feature.patch" >> ../../patches/series
```

### 4. Testing Patches

```bash
# Remove current patches
python3 scripts/patches.py reverse

# Reset to clean Chromium
cd chromium-src/src
git reset --hard HEAD
git clean -fd

# Test your new patch series
cd ../..
python3 scripts/patches.py apply --dry-run
python3 scripts/patches.py apply
```

### 5. Incremental Development

```bash
# Apply only core patches for faster iteration
python3 scripts/patches.py apply --series core

# Build and test
ninja -C chromium-src/src/out/DataSipper chrome

# Apply additional features
python3 scripts/patches.py apply --series extra
```

## Patch Development Best Practices

### Patch Structure

Each patch should:
- **Focus on single functionality** - One feature per patch
- **Include clear description** - Header with purpose and affected files
- **Follow Chromium conventions** - Code style, naming, architecture
- **Be self-contained** - Not depend on unpublished changes

### Patch Header Format

```patch
# DataSipper: Brief description of functionality
#
# Detailed description of what this patch does, why it's needed,
# and any important implementation details.
#
# Affects:
# - path/to/modified/file1.cc
# - path/to/modified/file2.h
# - path/to/new/directory/ (new directory)

--- a/existing/file.cc
+++ b/existing/file.cc
```

### File Organization

- **New files**: Create in appropriate `components/datasipper/` or `chrome/browser/datasipper/` directories
- **Modifications**: Minimal, surgical changes to existing Chromium files
- **Build files**: Update relevant `BUILD.gn` files for new components

### Testing Changes

```bash
# Quick validation
python3 scripts/patches.py validate

# Test patch application
python3 scripts/patches.py apply --dry-run

# Build test
ninja -C chromium-src/src/out/DataSipper chrome

# Runtime test
./chromium-src/src/out/DataSipper/chrome --enable-logging --v=1
```

## Advanced Workflows

### Selective Patch Application

```bash
# Apply only network interception (no UI)
python3 scripts/patches.py apply --series upstream-fixes
python3 scripts/patches.py apply --series core/datasipper
python3 scripts/patches.py apply --series core/network-interception

# Add UI later
python3 scripts/patches.py apply --series core/ui-panel
```

### Patch Debugging

```bash
# Verbose patch application
python3 scripts/patches.py apply --force 2>&1 | tee patch.log

# Check specific patch
cd chromium-src/src
patch -p1 --dry-run < ../../patches/core/datasipper/base-infrastructure.patch
```

### Upstream Updates

```bash
# Update Chromium to new version
cd chromium-src/src
git fetch origin
git checkout NEW_COMMIT_HASH
gclient sync

# Remove old patches
cd ../..
python3 scripts/patches.py reverse

# Test patch compatibility
python3 scripts/patches.py apply --dry-run

# Fix any conflicts
# Edit patches as needed

# Apply updated patches
python3 scripts/patches.py apply
```

## Integration with Build System

The patch system integrates with the development scripts:

```bash
# Full setup applies patches automatically
./scripts/dev-setup.sh

# Build configuration checks for applied patches
./scripts/configure-build.sh dev

# Status checking
./scripts/patches.py status
```

## Troubleshooting

### Common Issues

**Patch Application Failed**
```bash
# Check specific patch
cd chromium-src/src
patch -p1 --dry-run < ../../patches/failing-patch.patch

# Force application to see all errors
python3 scripts/patches.py apply --force
```

**Chromium Source Changes**
```bash
# Reset to clean state
cd chromium-src/src
git reset --hard HEAD
git clean -fd

# Reapply patches
cd ../..
python3 scripts/patches.py apply
```

**Build Errors After Patches**
```bash
# Check build configuration
gn check chromium-src/src/out/DataSipper

# Verify dependencies
gn desc chromium-src/src/out/DataSipper //components/datasipper
```

### Debug Mode

```bash
# Enable verbose logging in patches
export DATASIPPER_PATCH_DEBUG=1
python3 scripts/patches.py apply
```

## Continuous Integration

The patch system supports automated workflows:

```bash
# CI-friendly patch validation
python3 scripts/patches.py validate
if [ $? -ne 0 ]; then
  echo "Patch validation failed"
  exit 1
fi

# Automated patch application
python3 scripts/patches.py apply --no-commit
```

---

This patch management system ensures clean, maintainable modifications to Chromium while preserving the ability to integrate upstream updates efficiently.