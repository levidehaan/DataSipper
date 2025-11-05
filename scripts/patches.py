#!/usr/bin/env python3

"""
DataSipper Patch Management Script
Based on Ungoogled Chromium's patch management system
Handles application, validation, and management of patches
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import List, Optional


class PatchManager:
    """Manages DataSipper patches for Chromium source tree"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.patches_dir = project_root / "patches"
        self.series_file = self.patches_dir / "series"
        self.chromium_src = project_root / "chromium-src" / "src"
        
    def _run_command(self, cmd: List[str], cwd: Optional[Path] = None, 
                    check: bool = True) -> subprocess.CompletedProcess:
        """Run a shell command with error handling"""
        try:
            result = subprocess.run(
                cmd, 
                cwd=cwd or self.chromium_src,
                capture_output=True,
                text=True,
                check=check
            )
            return result
        except subprocess.CalledProcessError as e:
            print(f"Command failed: {' '.join(cmd)}")
            print(f"Error: {e.stderr}")
            raise
            
    def _find_patch_command(self) -> str:
        """Find the patch command in the system"""
        for cmd in ["patch", "gpatch"]:
            try:
                subprocess.run([cmd, "--version"], 
                             capture_output=True, check=True)
                return cmd
            except (subprocess.CalledProcessError, FileNotFoundError):
                continue
        raise RuntimeError("No patch command found (patch or gpatch)")
        
    def read_series_file(self) -> List[str]:
        """Read and parse the series file"""
        if not self.series_file.exists():
            raise FileNotFoundError(f"Series file not found: {self.series_file}")
            
        patches = []
        with open(self.series_file, 'r') as f:
            for line in f:
                line = line.strip()
                # Skip comments and empty lines
                if line and not line.startswith('#'):
                    # Extract patch name (ignore conditions)
                    patch_name = line.split()[0]
                    patches.append(patch_name)
        return patches
        
    def validate_patches(self) -> bool:
        """Validate that all patches in series file exist"""
        patches = self.read_series_file()
        missing_patches = []
        
        for patch in patches:
            patch_path = self.patches_dir / patch
            if not patch_path.exists():
                missing_patches.append(patch)
                
        if missing_patches:
            print("Missing patches:")
            for patch in missing_patches:
                print(f"  - {patch}")
            return False
            
        print(f"All {len(patches)} patches validated successfully")
        return True
        
    def check_git_status(self) -> bool:
        """Check if Chromium source is a clean git repository"""
        try:
            result = self._run_command(["git", "status", "--porcelain"])
            if result.stdout.strip():
                print("Warning: Chromium source has uncommitted changes")
                print("Consider committing or stashing changes before applying patches")
                return False
            return True
        except subprocess.CalledProcessError:
            print("Warning: Chromium source is not a git repository")
            return False
    
    def create_git_commit(self, message: str) -> bool:
        """Create a git commit after applying patches"""
        try:
            self._run_command(["git", "add", "."])
            self._run_command(["git", "commit", "-m", message])
            return True
        except subprocess.CalledProcessError as e:
            print(f"Failed to create git commit: {e.stderr}")
            return False

    def apply_patches(self, dry_run: bool = False, force: bool = False, 
                     series: Optional[str] = None, create_commit: bool = True) -> bool:
        """Apply patches to the Chromium source tree"""
        if not self.chromium_src.exists():
            raise FileNotFoundError(f"Chromium source not found: {self.chromium_src}")
            
        # Check git status before applying patches
        if not dry_run and not force:
            self.check_git_status()
            
        if not self.validate_patches():
            return False
            
        patches = self.read_series_file()
        
        # Filter patches by series if specified
        if series:
            patches = [p for p in patches if p.startswith(series)]
            if not patches:
                print(f"No patches found for series: {series}")
                return False
            print(f"Applying {len(patches)} patches from series: {series}")
        else:
            print(f"Applying {len(patches)} patches to {self.chromium_src}")
            
        patch_cmd = self._find_patch_command()
        applied_patches = []
        
        for i, patch in enumerate(patches, 1):
            patch_path = self.patches_dir / patch
            print(f"[{i}/{len(patches)}] Applying {patch}")
            
            cmd = [patch_cmd, "-p1"]
            if dry_run:
                cmd.append("--dry-run")
            if force:
                cmd.extend(["--forward", "--reject-file=-"])
                
            try:
                with open(patch_path, 'r') as patch_file:
                    result = subprocess.run(
                        cmd,
                        cwd=self.chromium_src,
                        stdin=patch_file,
                        capture_output=True,
                        text=True,
                        check=True
                    )
                    if result.stdout:
                        print(f"  ✓ Applied successfully")
                    applied_patches.append(patch)
            except subprocess.CalledProcessError as e:
                print(f"  ✗ Failed to apply {patch}")
                print(f"  Error: {e.stderr}")
                if not force:
                    print(f"Applied {len(applied_patches)}/{len(patches)} patches before failure")
                    return False
        
        # Create marker file and git commit
        if not dry_run and applied_patches:
            marker_file = self.chromium_src / ".datasipper_patches_applied"
            with open(marker_file, 'w') as f:
                f.write("# DataSipper patches applied\n")
                f.write(f"# Applied {len(applied_patches)} patches\n")
                for patch in applied_patches:
                    f.write(f"{patch}\n")
            
            if create_commit:
                commit_msg = f"Apply DataSipper patches ({len(applied_patches)} patches)"
                if series:
                    commit_msg += f" from series: {series}"
                self.create_git_commit(commit_msg)
                    
        print(f"✓ Patch application completed ({len(applied_patches)}/{len(patches)} applied)")
        return True
        
    def reverse_patches(self) -> bool:
        """Remove all applied patches"""
        if not self.chromium_src.exists():
            raise FileNotFoundError(f"Chromium source not found: {self.chromium_src}")
            
        patches = list(reversed(self.read_series_file()))
        patch_cmd = self._find_patch_command()
        
        print(f"Reversing {len(patches)} patches from {self.chromium_src}")
        
        for i, patch in enumerate(patches, 1):
            patch_path = self.patches_dir / patch
            print(f"[{i}/{len(patches)}] Reversing {patch}")
            
            cmd = [patch_cmd, "-p1", "-R"]
            
            try:
                with open(patch_path, 'r') as patch_file:
                    result = subprocess.run(
                        cmd,
                        cwd=self.chromium_src,
                        stdin=patch_file,
                        capture_output=True,
                        text=True,
                        check=True
                    )
            except subprocess.CalledProcessError as e:
                print(f"  Failed to reverse {patch}")
                print(f"  Error: {e.stderr}")
                # Continue with other patches
                
        print("Patch reversal completed")
        return True
        
    def create_patch(self, name: str, category: str = "core/datasipper") -> Path:
        """Create a new empty patch file"""
        patch_dir = self.patches_dir / category
        patch_dir.mkdir(parents=True, exist_ok=True)
        
        patch_path = patch_dir / f"{name}.patch"
        if patch_path.exists():
            raise FileExistsError(f"Patch already exists: {patch_path}")
            
        # Create empty patch with header
        with open(patch_path, 'w') as f:
            f.write(f"# DataSipper patch: {name}\n")
            f.write(f"# Category: {category}\n")
            f.write(f"# Description: \n")
            f.write(f"#\n")
            f.write(f"# --- a/path/to/file\n")
            f.write(f"# +++ b/path/to/file\n")
            f.write(f"# @@ -line,count +line,count @@\n")
            
        print(f"Created patch: {patch_path}")
        return patch_path
        
    def list_patches(self) -> None:
        """List all patches in the series"""
        patches = self.read_series_file()
        print(f"DataSipper patch series ({len(patches)} patches):")
        for i, patch in enumerate(patches, 1):
            patch_path = self.patches_dir / patch
            status = "✓" if patch_path.exists() else "✗"
            print(f"  {i:2d}. {status} {patch}")
    
    def get_patch_status(self) -> dict:
        """Get detailed status of patches and source tree"""
        status = {
            "chromium_src_exists": self.chromium_src.exists(),
            "patches_applied": False,
            "applied_patches": [],
            "git_clean": False,
            "total_patches": 0,
            "missing_patches": []
        }
        
        # Check if patches are applied
        marker_file = self.chromium_src / ".datasipper_patches_applied"
        if marker_file.exists():
            status["patches_applied"] = True
            with open(marker_file, 'r') as f:
                for line in f:
                    if not line.startswith('#') and line.strip():
                        status["applied_patches"].append(line.strip())
        
        # Check git status
        if self.chromium_src.exists():
            status["git_clean"] = self.check_git_status()
        
        # Get patch info
        try:
            patches = self.read_series_file()
            status["total_patches"] = len(patches)
            
            for patch in patches:
                patch_path = self.patches_dir / patch
                if not patch_path.exists():
                    status["missing_patches"].append(patch)
        except FileNotFoundError:
            pass
            
        return status
    
    def show_status(self) -> None:
        """Show detailed status of the DataSipper patch system"""
        status = self.get_patch_status()
        
        print("DataSipper Patch System Status")
        print("=" * 30)
        
        # Chromium source
        if status["chromium_src_exists"]:
            print("✓ Chromium source directory exists")
        else:
            print("✗ Chromium source directory not found")
            print(f"  Expected: {self.chromium_src}")
            
        # Patches
        if status["total_patches"] > 0:
            print(f"✓ Found {status['total_patches']} patches in series")
        else:
            print("✗ No patches found in series file")
            
        if status["missing_patches"]:
            print(f"⚠ {len(status['missing_patches'])} patches missing:")
            for patch in status["missing_patches"]:
                print(f"    - {patch}")
        
        # Applied patches
        if status["patches_applied"]:
            print(f"✓ DataSipper patches applied ({len(status['applied_patches'])} patches)")
            if len(status["applied_patches"]) != status["total_patches"]:
                print("⚠ Applied patch count doesn't match series file")
        else:
            print("○ DataSipper patches not applied")
            
        # Git status
        if status["git_clean"]:
            print("✓ Git working directory is clean")
        else:
            print("⚠ Git working directory has changes")
            
        print()
        print("Next steps:")
        if not status["chromium_src_exists"]:
            print("  1. Run: ./scripts/fetch-chromium.sh")
        elif not status["patches_applied"]:
            print("  1. Run: python3 scripts/patches.py apply")
        else:
            print("  1. Configure build: ./scripts/configure-build.sh dev")
            print("  2. Build DataSipper: ninja -C chromium-src/src/out/DataSipper chrome")


def main():
    parser = argparse.ArgumentParser(description="DataSipper Patch Management")
    parser.add_argument("--project-root", type=Path, 
                       default=Path(__file__).parent.parent,
                       help="Project root directory")
    
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Apply patches
    apply_parser = subparsers.add_parser("apply", help="Apply patches")
    apply_parser.add_argument("--dry-run", action="store_true",
                             help="Test patch application without applying")
    apply_parser.add_argument("--force", action="store_true",
                             help="Force application, continue on errors")
    apply_parser.add_argument("--series", type=str,
                             help="Apply only patches from specific series (e.g., 'core', 'extra')")
    apply_parser.add_argument("--no-commit", action="store_true",
                             help="Don't create git commit after applying patches")
    
    # Reverse patches
    subparsers.add_parser("reverse", help="Remove all applied patches")
    
    # Validate patches
    subparsers.add_parser("validate", help="Validate all patches exist")
    
    # List patches
    subparsers.add_parser("list", help="List all patches in series")
    
    # Show status
    subparsers.add_parser("status", help="Show patch system status")
    
    # Create new patch
    create_parser = subparsers.add_parser("create", help="Create new patch")
    create_parser.add_argument("name", help="Patch name")
    create_parser.add_argument("--category", default="core/datasipper",
                              help="Patch category (default: core/datasipper)")
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
        
    manager = PatchManager(args.project_root)
    
    try:
        if args.command == "apply":
            success = manager.apply_patches(
                dry_run=args.dry_run, 
                force=args.force,
                series=args.series,
                create_commit=not args.no_commit
            )
            return 0 if success else 1
        elif args.command == "reverse":
            success = manager.reverse_patches()
            return 0 if success else 1
        elif args.command == "validate":
            success = manager.validate_patches()
            return 0 if success else 1
        elif args.command == "list":
            manager.list_patches()
            return 0
        elif args.command == "status":
            manager.show_status()
            return 0
        elif args.command == "create":
            manager.create_patch(args.name, args.category)
            return 0
    except Exception as e:
        print(f"Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())