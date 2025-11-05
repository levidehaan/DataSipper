# DataSipper Patch Development Guide

This document describes how to develop, manage, and maintain patches for the DataSipper project.

## Overview

DataSipper uses a patch-based approach to modify Chromium, similar to projects like Ungoogled Chromium. This allows us to maintain our modifications separately from the upstream Chromium codebase and makes it easier to update to newer Chromium versions.

## Patch Organization

### Directory Structure
```
patches/
├── series                          # Master patch application order
├── core/                          # Essential DataSipper functionality
│   ├── datasipper/               # Core infrastructure patches
│   ├── network-interception/     # Network capture patches
│   ├── ui-panel/                 # UI panel patches
│   └── external-integrations/    # Integration patches
├── extra/                        # Optional features
│   ├── network-interception/     # Advanced network features
│   ├── ui-panel/                 # Enhanced UI features
│   └── external-integrations/    # Additional integrations
└── upstream-fixes/               # Fixes for upstream Chromium issues
```

### Patch Categories

1. **Core Patches** (`core/`): Essential modifications required for DataSipper functionality
   - Must be maintained across Chromium updates
   - Should be minimal and focused
   - Include network interception, UI panel, and basic integrations

2. **Extra Patches** (`extra/`): Optional enhancements and advanced features
   - May be dropped if they become too difficult to maintain
   - Include advanced filtering, visualization, and extended integrations

3. **Upstream Fixes** (`upstream-fixes/`): Patches that fix issues in upstream Chromium
   - Should be submitted upstream when possible
   - May be removed when fixes are included in newer Chromium versions

## Tools and Environment

### Required Tools
- GNU Quilt: For patch development and management
- Python 3.9+: For patch automation scripts
- Git: For version control and upstream tracking

### Environment Setup
```bash
# Source the quilt environment
source scripts/set_quilt_vars.sh

# Navigate to Chromium source
cd chromium-src/src
```

## Patch Development Workflow

### 1. Creating a New Patch

```bash
# Create new patch file
./scripts/patches.py create my-feature-name --category core/datasipper

# Or using quilt (after sourcing environment)
cd chromium-src/src
qnew core/datasipper/my-feature-name.patch
```

### 2. Making Changes

```bash
# Add files you plan to modify
quilt add path/to/file.cc
quilt add path/to/file.h

# Make your changes using your preferred editor
quilt edit path/to/file.cc

# Or edit directly and add afterwards
vim path/to/file.cc
quilt add path/to/file.cc
```

### 3. Updating the Patch

```bash
# Refresh the patch with your changes
quilt refresh

# Review the patch
quilt diff
```

### 4. Adding to Series File

Edit `patches/series` to include your new patch in the correct location:
```
# Add your patch in the appropriate section
core/datasipper/my-feature-name.patch
```

### 5. Testing the Patch

```bash
# Test patch application
./scripts/patches.py apply --dry-run

# Apply all patches
./scripts/patches.py apply

# Build and test Chromium
cd chromium-src/src
ninja -C out/Default chrome
```

## Patch Management Commands

### Using the Python Script

```bash
# List all patches
./scripts/patches.py list

# Validate all patches exist
./scripts/patches.py validate

# Apply all patches
./scripts/patches.py apply

# Test application without applying
./scripts/patches.py apply --dry-run

# Force application (continue on errors)
./scripts/patches.py apply --force

# Remove all patches
./scripts/patches.py reverse

# Create new patch
./scripts/patches.py create patch-name --category core/datasipper
```

### Using Quilt Directly

```bash
# Source environment first
source scripts/set_quilt_vars.sh
cd chromium-src/src

# Apply patches
qpush -a                    # Apply all patches
qpush                       # Apply next patch
qpush patch-name            # Apply up to specific patch

# Remove patches  
qpop -a                     # Remove all patches
qpop                        # Remove current patch
qpop patch-name             # Remove down to specific patch

# Patch development
qnew patch-name             # Create new patch
qadd file                   # Add file to current patch
qedit file                  # Edit file (auto-adds to patch)
qrefresh                    # Update current patch
qdiff                       # Show current patch diff

# Information
qtop                        # Show current patch
qapplied                    # List applied patches
qunapplied                  # List unapplied patches
qseries                     # Show all patches
```

## Patch Format Requirements

### File Format
- Must use unified diff format (`diff -u`)
- UTF-8 encoding required
- Paths must be relative to Chromium source root
- Use `a/` and `b/` prefixes with 3-line context

### Naming Conventions
- Use descriptive names: `network-request-interception.patch`
- Include feature area: `ui-panel-slide-animation.patch`
- Use hyphens, not underscores
- End with `.patch` extension

### Patch Headers
Include descriptive headers in patches:
```diff
# DataSipper: Add network request interception
# 
# This patch implements the core network request interception
# functionality for capturing HTTP/HTTPS traffic.
#
# Affects:
# - content/browser/loader/navigation_url_loader_impl.cc
# - services/network/url_loader.cc

--- a/content/browser/loader/navigation_url_loader_impl.cc
+++ b/content/browser/loader/navigation_url_loader_impl.cc
```

## Handling Chromium Updates

### 1. Update Process
```bash
# Update Chromium source to new version
cd chromium-src/src
git fetch origin
git checkout <new-commit-hash>
gclient sync

# Remove old patches
./scripts/patches.py reverse

# Attempt to apply patches
./scripts/patches.py apply
```

### 2. Resolving Conflicts
When patches fail to apply:

```bash
# Try forcing application to see which patches fail
./scripts/patches.py apply --force

# For each failing patch, use quilt to fix
cd chromium-src/src
source ../../scripts/set_quilt_vars.sh

# Push patches one by one and fix conflicts
qpush                       # Apply next patch
# If it fails:
qpush -f                    # Force apply with rejects
qedit <conflicted-file>     # Edit to fix conflicts
qrefresh                    # Update the patch
```

### 3. Updating Patch Content
```bash
# Make necessary changes to fix the patch
quilt edit path/to/file.cc

# Update the patch
quilt refresh

# Continue with remaining patches
quilt push
```

## Best Practices

### 1. Patch Design
- Keep patches focused and minimal
- Avoid large code deletions using language features
- Remove code line by line for clarity
- Group related changes in single patches
- Document the purpose and scope of each patch

### 2. Testing
- Always test patch application on clean Chromium source
- Build and test functionality after applying patches
- Test patch removal and reapplication
- Verify patches apply in correct order

### 3. Maintenance
- Regularly test patches against newer Chromium versions
- Keep patch descriptions up to date
- Remove patches that are no longer needed
- Submit upstream fixes when appropriate

### 4. Documentation
- Document patch purpose and implementation
- Keep series file organized and commented
- Update this guide when adding new procedures
- Maintain patch interdependency documentation

## Common Issues and Solutions

### Patch Application Failures
- **Fuzz**: Code has changed slightly, patch may still work
- **Rejects**: Code has changed significantly, manual intervention needed
- **Missing files**: Files have been moved or deleted upstream

### Resolution Strategies
1. Update file paths if files have moved
2. Adapt patches to new code structure
3. Split large patches into smaller, more focused ones
4. Remove patches that are no longer relevant

### Debugging Tips
- Use `--dry-run` to test without applying
- Check git log for relevant upstream changes
- Use `quilt diff` to see what changes a patch makes
- Apply patches incrementally to isolate issues

## Contributing Patches

When contributing new patches:

1. Follow the patch format requirements
2. Test thoroughly on clean Chromium source
3. Document the patch purpose and scope
4. Add appropriate entries to the series file
5. Update relevant documentation
6. Consider backward compatibility with existing features

For questions or issues with patch development, consult the project documentation or create an issue in the project repository.