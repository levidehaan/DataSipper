#!/bin/bash

# DataSipper - Pull Changes Script
# Safely pulls changes from GitHub origin with divergent branch handling

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if we're in a git repository
check_git_repo() {
    if ! git rev-parse --git-dir > /dev/null 2>&1; then
        print_error "Not in a git repository!"
        exit 1
    fi
}

# Function to get current branch
get_current_branch() {
    git branch --show-current
}

# Function to check if working directory is clean
check_working_directory() {
    if ! git diff-index --quiet HEAD --; then
        print_warning "Working directory has uncommitted changes!"
        echo "Modified files:"
        git status --porcelain
        echo
        read -p "Do you want to continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_error "Aborting due to uncommitted changes."
            echo "Please commit or stash your changes first."
            exit 1
        fi
    fi
}

# Function to fetch latest changes
fetch_changes() {
    print_status "Fetching latest changes from origin..."
    git fetch origin
    print_success "Fetch completed."
}

# Function to check for divergent branches
check_divergence() {
    local current_branch=$(get_current_branch)
    local upstream="origin/$current_branch"
    
    # Check if upstream branch exists
    if ! git rev-parse --verify "$upstream" > /dev/null 2>&1; then
        print_warning "Upstream branch '$upstream' does not exist."
        print_status "Creating tracking relationship with origin/$current_branch"
        git branch --set-upstream-to=origin/$current_branch $current_branch 2>/dev/null || true
        return 1
    fi
    
    local ahead=$(git rev-list --count HEAD..$upstream 2>/dev/null || echo "0")
    local behind=$(git rev-list --count $upstream..HEAD 2>/dev/null || echo "0")
    
    echo "Branch status:"
    echo "  - Local commits ahead of remote: $behind"
    echo "  - Remote commits ahead of local: $ahead"
    echo
    
    if [ "$ahead" -gt 0 ] && [ "$behind" -gt 0 ]; then
        return 0  # Divergent
    elif [ "$ahead" -gt 0 ]; then
        return 2  # Behind remote
    elif [ "$behind" -gt 0 ]; then
        return 3  # Ahead of remote
    else
        return 4  # Up to date
    fi
}

# Function to handle pull with user choice
handle_pull() {
    local current_branch=$(get_current_branch)
    
    check_divergence
    local divergence_status=$?
    
    case $divergence_status in
        0)  # Divergent branches
            print_warning "Branches have diverged!"
            echo
            echo "Choose how to reconcile:"
            echo "1) Merge (creates a merge commit)"
            echo "2) Rebase (replays your commits on top of remote)"
            echo "3) Fast-forward only (fails if not possible)"
            echo "4) Show diff and exit"
            echo "5) Cancel"
            echo
            read -p "Enter your choice (1-5): " -n 1 -r choice
            echo
            
            case $choice in
                1)
                    print_status "Performing merge..."
                    git pull --no-ff origin $current_branch
                    print_success "Merge completed successfully!"
                    ;;
                2)
                    print_status "Performing rebase..."
                    git pull --rebase origin $current_branch
                    print_success "Rebase completed successfully!"
                    ;;
                3)
                    print_status "Attempting fast-forward only..."
                    git pull --ff-only origin $current_branch
                    print_success "Fast-forward completed successfully!"
                    ;;
                4)
                    print_status "Showing differences between local and remote..."
                    echo
                    echo "=== Commits in remote not in local ==="
                    git log --oneline HEAD..origin/$current_branch
                    echo
                    echo "=== Commits in local not in remote ==="
                    git log --oneline origin/$current_branch..HEAD
                    echo
                    print_status "No changes made. Exiting."
                    exit 0
                    ;;
                5)
                    print_status "Operation cancelled by user."
                    exit 0
                    ;;
                *)
                    print_error "Invalid choice. Exiting."
                    exit 1
                    ;;
            esac
            ;;
        1)  # No upstream or error
            print_warning "Could not determine upstream status. Attempting simple pull..."
            git pull origin $current_branch
            ;;
        2)  # Behind remote
            print_status "Local branch is behind remote. Performing fast-forward..."
            git pull --ff-only origin $current_branch
            print_success "Fast-forward completed successfully!"
            ;;
        3)  # Ahead of remote
            print_status "Local branch is ahead of remote. Nothing to pull."
            echo "You may want to push your changes: git push origin $current_branch"
            ;;
        4)  # Up to date
            print_success "Already up to date!"
            ;;
        *)
            print_error "Unknown divergence status. Exiting."
            exit 1
            ;;
    esac
}

# Function to show final status
show_final_status() {
    echo
    print_status "Final repository status:"
    git status --short
    echo
    print_status "Last 5 commits:"
    git log --oneline -5
}

# Main execution
main() {
    echo "=== DataSipper Git Pull Script ==="
    echo
    
    # Check prerequisites
    check_git_repo
    
    local current_branch=$(get_current_branch)
    print_status "Current branch: $current_branch"
    
    # Check working directory
    check_working_directory
    
    # Fetch and handle pull
    fetch_changes
    echo
    handle_pull
    
    # Show final status
    show_final_status
    
    print_success "Pull operation completed!"
}

# Handle script arguments
case "${1:-}" in
    --help|-h)
        echo "DataSipper Git Pull Script"
        echo
        echo "Usage: $0 [options]"
        echo
        echo "Options:"
        echo "  --help, -h    Show this help message"
        echo "  --force       Skip working directory check"
        echo
        echo "This script safely pulls changes from GitHub origin,"
        echo "handling divergent branches with user input."
        exit 0
        ;;
    --force)
        FORCE_PULL=true
        ;;
    "")
        # No arguments, proceed normally
        ;;
    *)
        print_error "Unknown option: $1"
        echo "Use --help for usage information."
        exit 1
        ;;
esac

# Run main function
main 