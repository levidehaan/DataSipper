#!/bin/bash
# Script for managing DataSipper patches against Chromium source.
# Based on the comprehensive DataSipper development plan

CHROMIUM_SRC_DIR="/workspace/src"
PATCHES_DIR="/workspace/patches"
SERIES_FILE="$PATCHES_DIR/series"
CHROMIUM_TAG_VALUE=$(grep CHROMIUM_TAG /workspace/CHROMIUM_VERSION.txt | cut -d= -f2)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Ensure script is executed from the project root or adjust paths accordingly
check_environment() {
    if [[ ! -d "$CHROMIUM_SRC_DIR" ]] || [[ ! -d "$PATCHES_DIR" ]]; then
        log_error "Required directories not found."
        log_error "Expected: $CHROMIUM_SRC_DIR and $PATCHES_DIR"
        log_error "Current directory: $(pwd)"
        log_error "Run from project root or check paths."
        exit 1
    fi

    if [[ ! -f "/workspace/CHROMIUM_VERSION.txt" ]]; then
        log_error "CHROMIUM_VERSION.txt not found. This file should contain the target Chromium version."
        exit 1
    fi

    if [[ -z "$CHROMIUM_TAG_VALUE" ]]; then
        log_error "Could not determine CHROMIUM_TAG from CHROMIUM_VERSION.txt"
        exit 1
    fi

    log_info "Environment check passed"
    log_info "Chromium source: $CHROMIUM_SRC_DIR"
    log_info "Patches directory: $PATCHES_DIR"
    log_info "Target Chromium version: $CHROMIUM_TAG_VALUE"
}

apply_all_patches() {
    log_info "Applying all patches..."
    cd "$CHROMIUM_SRC_DIR" || exit 1
    
    # Ensure clean state before applying
    log_info "Resetting to clean state..."
    git reset --hard HEAD 2>/dev/null # Reset any uncommitted changes in tracked files
    git clean -fdx 2>/dev/null         # Remove untracked files and directories

    if [[ ! -f "$SERIES_FILE" ]]; then
        log_warn "Series file '$SERIES_FILE' not found. No patches to apply."
        return
    fi

    local patches_applied=0
    local patches_failed=0

    while IFS= read -r patch_file || [[ -n "$patch_file" ]]; do
        # Skip empty lines and comments
        if [[ -z "$patch_file" ]] || [[ "$patch_file" == \#* ]]; then
            continue
        fi

        local patch_path="$PATCHES_DIR/$patch_file"
        if [[ ! -f "$patch_path" ]]; then
            log_error "Patch file not found: $patch_path"
            ((patches_failed++))
            continue
        fi

        log_info "Applying $patch_file..."
        if patch -p1 --no-backup-if-mismatch < "$patch_path"; then
            log_info "$patch_file applied successfully."
            ((patches_applied++))
        else
            log_error "Failed to apply $patch_file."
            log_error "You may need to resolve conflicts manually or update the patch."
            
            # Show which files failed
            log_info "Checking which files were affected..."
            patch -p1 --dry-run < "$patch_path" 2>&1 | grep -E "(FAILED|can't find file)" || true
            
            ((patches_failed++))
            
            # Ask user if they want to continue
            echo
            read -p "Continue with remaining patches? (y/N): " -n 1 -r
            echo
            if [[ ! $REPLY =~ ^[Yy]$ ]]; then
                log_error "Patch application aborted by user."
                exit 1
            fi
        fi
    done < "$SERIES_FILE"

    echo
    log_info "Patch application complete:"
    log_info "  Applied: $patches_applied"
    [[ $patches_failed -gt 0 ]] && log_error "  Failed: $patches_failed" || log_info "  Failed: $patches_failed"
    
    if [[ $patches_failed -gt 0 ]]; then
        log_warn "Some patches failed to apply. Check the errors above."
        return 1
    fi
}

unapply_all_patches() {
    log_info "Unapplying all patches (resetting to base Chromium tag: $CHROMIUM_TAG_VALUE)..."
    cd "$CHROMIUM_SRC_DIR" || exit 1
    
    # Check if we're in a git repository
    if ! git rev-parse --git-dir >/dev/null 2>&1; then
        log_error "Not in a git repository. Cannot reset to base tag."
        exit 1
    fi
    
    # Check if the tag exists
    if ! git rev-parse --verify "refs/tags/$CHROMIUM_TAG_VALUE" >/dev/null 2>&1; then
        log_error "Tag $CHROMIUM_TAG_VALUE not found in repository."
        log_info "Available tags matching pattern:"
        git tag -l "*7151*" | head -10
        exit 1
    fi
    
    log_info "Resetting to tag: $CHROMIUM_TAG_VALUE"
    git reset --hard "refs/tags/$CHROMIUM_TAG_VALUE"
    git clean -fdx # Clean thoroughly
    
    log_info "Repository reset to $CHROMIUM_TAG_VALUE. Patches effectively unapplied."
}

generate_patch() {
    local patch_name="$1"
    local commit_message="$2"
    
    if [[ -z "$patch_name" ]] || [[ -z "$commit_message" ]]; then
        log_error "Usage: generate_patch <patch_filename.patch> \"<Commit message>\""
        exit 1
    fi

    cd "$CHROMIUM_SRC_DIR" || exit 1

    # Check for staged changes
    if git diff --cached --quiet; then
        log_error "No changes staged to commit. Stage your changes first with 'git add'."
        log_info "Example: git add path/to/modified/file.cc"
        exit 1
    fi

    local patch_path="$PATCHES_DIR/$patch_name"
    
    # Create patch directory if it doesn't exist
    mkdir -p "$(dirname "$patch_path")"
    
    # Generate the patch
    log_info "Generating patch: $patch_name"
    git diff --cached > "$patch_path"
    
    # Add to series file if not already present
    if [[ -f "$SERIES_FILE" ]] && ! grep -Fxq "$patch_name" "$SERIES_FILE"; then
        echo "$patch_name" >> "$SERIES_FILE"
        log_info "Added $patch_name to series file."
    elif [[ ! -f "$SERIES_FILE" ]]; then
        echo "$patch_name" > "$SERIES_FILE"
        log_info "Created series file with $patch_name."
    fi
    
    # Commit locally to track changes on the branch
    git commit -m "$commit_message"
    
    log_info "Generated patch $patch_path and added to series file."
    log_info "Changes also committed to the local branch."
    
    # Show patch statistics
    local lines_added=$(grep -c "^+" "$patch_path" || echo "0")
    local lines_removed=$(grep -c "^-" "$patch_path" || echo "0")
    log_info "Patch statistics: +$lines_added -$lines_removed lines"
}

validate_patches() {
    log_info "Validating all patches..."
    
    if [[ ! -f "$SERIES_FILE" ]]; then
        log_error "Series file not found: $SERIES_FILE"
        exit 1
    fi
    
    local total_patches=0
    local valid_patches=0
    local invalid_patches=0
    
    while IFS= read -r patch_file || [[ -n "$patch_file" ]]; do
        # Skip empty lines and comments
        if [[ -z "$patch_file" ]] || [[ "$patch_file" == \#* ]]; then
            continue
        fi
        
        ((total_patches++))
        local patch_path="$PATCHES_DIR/$patch_file"
        
        if [[ ! -f "$patch_path" ]]; then
            log_error "Missing patch file: $patch_path"
            ((invalid_patches++))
            continue
        fi
        
        # Check if patch has valid format
        if grep -q "^--- " "$patch_path" && grep -q "^+++ " "$patch_path"; then
            log_info "✓ $patch_file"
            ((valid_patches++))
        else
            log_error "✗ $patch_file (invalid format)"
            ((invalid_patches++))
        fi
        
    done < "$SERIES_FILE"
    
    echo
    log_info "Validation complete:"
    log_info "  Total patches: $total_patches"
    log_info "  Valid: $valid_patches"
    [[ $invalid_patches -gt 0 ]] && log_error "  Invalid: $invalid_patches" || log_info "  Invalid: $invalid_patches"
    
    if [[ $invalid_patches -gt 0 ]]; then
        return 1
    fi
}

list_patches() {
    log_info "DataSipper patch series:"
    
    if [[ ! -f "$SERIES_FILE" ]]; then
        log_warn "No series file found: $SERIES_FILE"
        return
    fi
    
    local patch_num=1
    while IFS= read -r patch_file || [[ -n "$patch_file" ]]; do
        # Skip empty lines and comments
        if [[ -z "$patch_file" ]] || [[ "$patch_file" == \#* ]]; then
            if [[ "$patch_file" == \#* ]]; then
                echo "    $patch_file"
            fi
            continue
        fi
        
        local patch_path="$PATCHES_DIR/$patch_file"
        local status="✗"
        local size="N/A"
        
        if [[ -f "$patch_path" ]]; then
            status="✓"
            size=$(wc -l < "$patch_path")
        fi
        
        printf "%2d. %s %s (%s lines)\n" "$patch_num" "$status" "$patch_file" "$size"
        ((patch_num++))
        
    done < "$SERIES_FILE"
}

dry_run() {
    log_info "Performing dry-run of patch application..."
    cd "$CHROMIUM_SRC_DIR" || exit 1
    
    if [[ ! -f "$SERIES_FILE" ]]; then
        log_warn "Series file '$SERIES_FILE' not found. No patches to test."
        return
    fi

    local patches_ok=0
    local patches_fail=0

    while IFS= read -r patch_file || [[ -n "$patch_file" ]]; do
        # Skip empty lines and comments
        if [[ -z "$patch_file" ]] || [[ "$patch_file" == \#* ]]; then
            continue
        fi

        local patch_path="$PATCHES_DIR/$patch_file"
        if [[ ! -f "$patch_path" ]]; then
            log_error "Patch file not found: $patch_path"
            ((patches_fail++))
            continue
        fi

        log_info "Testing $patch_file..."
        if patch -p1 --dry-run < "$patch_path" >/dev/null 2>&1; then
            log_info "  ✓ Would apply cleanly"
            ((patches_ok++))
        else
            log_error "  ✗ Would fail to apply"
            ((patches_fail++))
            
            # Show details of what would fail
            patch -p1 --dry-run < "$patch_path" 2>&1 | grep -E "(FAILED|can't find file)" | head -3
        fi
    done < "$SERIES_FILE"

    echo
    log_info "Dry-run complete:"
    log_info "  Patches that would apply: $patches_ok"
    [[ $patches_fail -gt 0 ]] && log_error "  Patches that would fail: $patches_fail" || log_info "  Patches that would fail: $patches_fail"
    
    if [[ $patches_fail -gt 0 ]]; then
        return 1
    fi
}

show_help() {
    cat << EOF
DataSipper Patch Management Script

Usage: $0 [COMMAND] [OPTIONS]

Commands:
    apply                   Apply all patches from series file
    unapply                Reset to base Chromium (remove all patches)
    generate <file> <msg>   Generate patch from staged changes
    validate                Validate all patches in series
    list                    List all patches in series
    dry-run                 Test patch application without applying
    help                    Show this help message

Examples:
    $0 apply
    $0 generate core/datasipper/my-feature.patch "Add new DataSipper feature"
    $0 validate
    $0 dry-run

Environment:
    CHROMIUM_SRC_DIR: $CHROMIUM_SRC_DIR
    PATCHES_DIR:      $PATCHES_DIR
    SERIES_FILE:      $SERIES_FILE
    CHROMIUM_TAG:     $CHROMIUM_TAG_VALUE

EOF
}

# Main script logic
main() {
    case "$1" in
        apply)
            check_environment
            apply_all_patches
            ;;
        unapply)
            check_environment
            unapply_all_patches
            ;;
        generate)
            if [[ $# -ne 3 ]]; then
                log_error "Usage: $0 generate <patch_filename.patch> \"<Commit Message>\""
                exit 1
            fi
            check_environment
            generate_patch "$2" "$3"
            ;;
        validate)
            check_environment
            validate_patches
            ;;
        list)
            list_patches
            ;;
        dry-run)
            check_environment
            dry_run
            ;;
        help|--help|-h)
            show_help
            ;;
        "")
            log_error "No command specified."
            show_help
            exit 1
            ;;
        *)
            log_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"