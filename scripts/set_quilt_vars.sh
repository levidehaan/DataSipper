#!/bin/bash

# DataSipper Quilt Environment Configuration
# This script sets up the environment for using GNU Quilt with DataSipper patches

# Get the script directory (project root)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Set quilt patches directory
export QUILT_PATCHES="${PROJECT_ROOT}/patches"

# Quilt configuration for better display
export QUILT_DIFF_ARGS="--no-timestamps --no-index -p ab --color=auto"
export QUILT_REFRESH_ARGS="--no-timestamps --no-index -p ab"
export QUILT_COLORS="diff_hdr=1;32:diff_add=1;34:diff_rem=1;31:diff_hunk=1;33:diff_ctx=35:diff_cctx=33"

# Enable colored output
export QUILT_PATCH_OPTS="--unified=3 --show-c-function"

# Set up aliases for common quilt operations
alias qpush='quilt push'
alias qpop='quilt pop'
alias qtop='quilt top'
alias qapplied='quilt applied'
alias qunapplied='quilt unapplied'
alias qnew='quilt new'
alias qrefresh='quilt refresh'
alias qdiff='quilt diff'
alias qedit='quilt edit'
alias qadd='quilt add'
alias qremove='quilt remove'
alias qseries='quilt series'

echo "Quilt environment configured for DataSipper"
echo "QUILT_PATCHES=${QUILT_PATCHES}"
echo ""
echo "Available aliases:"
echo "  qpush, qpop, qtop, qapplied, qunapplied"
echo "  qnew, qrefresh, qdiff, qedit, qadd, qremove, qseries"
echo ""
echo "Usage:"
echo "  cd chromium-src/src"
echo "  qpush -a    # Apply all patches"
echo "  qpop -a     # Remove all patches"
echo "  qnew name   # Create new patch"
echo "  qrefresh    # Update current patch"