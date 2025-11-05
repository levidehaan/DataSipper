# DataSipper Chromium Build Strategy

## Problem Analysis

Based on the build failures, we've identified:

1. **Clang Compiler Crashes**: The specific Chromium commit `fb224f9793306` triggers segfaults in clang during the `ReachingDefAnalysis` optimization pass
2. **Generated Code Issues**: The `network_context.mojom-blink.cc` file causes compiler crashes
3. **Environment Sensitivity**: The build is highly sensitive to compiler versions and build configurations

## Solution Strategy

### 1. Docker-Based Controlled Environment
- **Ubuntu 22.04 base**: Stable, well-tested environment
- **Controlled dependencies**: Fixed versions of build tools
- **Isolated build**: Prevents host system interference

### 2. Multi-Configuration Approach
We try multiple build configurations in order of stability:

#### Configuration 1: Safe Minimal Build
```bash
is_debug=false
is_component_build=false
symbol_level=0
enable_nacl=false
treat_warnings_as_errors=false
use_goma=false
```
- Static linking reduces complexity
- No debug symbols (fastest build)
- Minimal features to avoid problematic code paths

#### Configuration 2: Component Build
```bash
is_debug=false
is_component_build=true
symbol_level=1
enable_nacl=false
treat_warnings_as_errors=false
use_goma=false
```
- Shared libraries for faster incremental builds
- Minimal debug symbols for debugging if needed

#### Configuration 3: Debug Build
```bash
is_debug=true
is_component_build=true
symbol_level=1
enable_nacl=false
treat_warnings_as_errors=false
optimize_webui=false
```
- Full debug build with reduced optimizations
- May avoid optimizer bugs

### 3. Progressive Building
1. **Test with base library**: Quick validation of build environment
2. **Build chrome target**: Full browser build
3. **Validate binary**: Ensure the executable works

### 4. Alternative Commit Strategy
If the target commit fails, we automatically try:
- Recent commits from the same branch (`refs/branch-heads/7151`)
- Look for stable commits near the target date

### 5. Comprehensive Logging
- **Real-time logging**: All output captured and displayed
- **Error extraction**: Failed commands highlighted
- **Build summaries**: Clear success/failure reporting
- **Log preservation**: All logs saved for analysis

## Usage

### Quick Start
```bash
# Run the complete build process
./docker/docker-build.sh
```

### What It Does
1. **Builds Docker image** with controlled Chromium build environment
2. **Fetches Chromium source** with the target commit
3. **Tries multiple build configurations** until one succeeds
4. **Provides detailed logging** and error analysis
5. **Extracts binaries** if successful

### Outputs
- `build-logs/`: All build logs and summaries
- **Success**: Instructions for next steps (applying DataSipper patches)
- **Failure**: Detailed error analysis and recommendations

## Troubleshooting

### Common Issues
1. **Compiler crashes**: Try different optimization levels
2. **Memory issues**: Ensure Docker has enough memory (8GB+)
3. **Disk space**: Chromium builds require 100GB+ free space
4. **Network issues**: Depot_tools requires internet access

### Log Analysis
- `chromium-build-*.log`: Real-time build output
- `build-full.log`: Complete build log from inside container
- `build-errors.log`: Extracted error messages
- `build-summary.log`: Condensed error summary

## Next Steps After Successful Build

1. **Extract Chrome binary** from Docker container
2. **Test basic functionality** (`chrome --version`)
3. **Apply DataSipper patches** to the source
4. **Rebuild with DataSipper features**
5. **Test DataSipper functionality**

## Advantages of This Approach

1. **Reproducible**: Same environment every time
2. **Isolated**: Won't affect host system
3. **Robust**: Multiple fallback strategies
4. **Informative**: Clear logging and error reporting
5. **Automated**: One command to run everything