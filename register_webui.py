#!/usr/bin/env python3
"""
DataSipper WebUI Registration Script

This script automatically registers the DataSipper WebUI in Chrome's build system.
It makes minimal, surgical edits to 3 files to enable chrome://datasipper/

Run from chromium-src/src/ directory:
    python3 ../../register_webui.py
"""

import os
import sys
import shutil
from pathlib import Path

def backup_file(filepath):
    """Create a backup of the file before modifying."""
    backup_path = f"{filepath}.backup"
    shutil.copy2(filepath, backup_path)
    print(f"✓ Backed up: {backup_path}")
    return backup_path

def register_in_factory(base_path):
    """Register DataSipper in chrome_web_ui_controller_factory.cc"""
    filepath = base_path / "chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc"

    if not filepath.exists():
        print(f"❌ File not found: {filepath}")
        return False

    backup_file(filepath)

    with open(filepath, 'r') as f:
        content = f.read()

    # Add include
    include_marker = '#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)\n#include "chrome/browser/ui/webui/extensions/extensions_ui.h"\n#endif'
    include_addition = '''#if BUILDFLAG(ENABLE_DATASIPPER)
#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_ui.h"
#endif

'''

    if '#include "chrome/browser/ui/webui/side_panel/datasipper/datasipper_ui.h"' in content:
        print("⚠ DataSipper include already present in factory")
    else:
        content = content.replace(include_marker, include_marker + '\n\n' + include_addition)
        print("✓ Added DataSipper include")

    # Add factory function (find GetWebUIFactoryFunction and add registration)
    factory_marker = '  return nullptr;\n#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)\n}'
    factory_addition = '''

#if BUILDFLAG(ENABLE_DATASIPPER)
WebUIFactoryFunction GetDataSipperFactory(const GURL& url) {
  if (url.host_piece() == chrome::kChromeUIDataSipperHost) {
    return &NewWebUI<DataSipperUI>;
  }
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_DATASIPPER)

// Add after devtools check in GetWebUIFactoryFunction
// if (auto* func = GetDataSipperFactory(url)) return func;
'''

    if 'GetDataSipperFactory' not in content:
        # Add the factory function
        content = content.replace(factory_marker, factory_marker + factory_addition)

        # Now find the GetWebUIFactoryFunction and add the call
        func_start = content.find('WebUIFactoryFunction GetWebUIFactoryFunction')
        func_end = content.find('  return nullptr;\n#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)\n}', func_start)

        if func_start != -1 and func_end != -1:
            # Insert before the final return
            insertion_point = func_end
            call_addition = '''
#if BUILDFLAG(ENABLE_DATASIPPER)
  if (auto* func = GetDataSipperFactory(url)) {
    return func;
  }
#endif

'''
            content = content[:insertion_point] + call_addition + content[insertion_point:]
            print("✓ Registered DataSipper factory function")
        else:
            print("⚠ Could not find insertion point for factory call")
    else:
        print("⚠ DataSipper factory already registered")

    with open(filepath, 'w') as f:
        f.write(content)

    return True

def add_to_ui_build(base_path):
    """Add DataSipper to chrome/browser/ui/BUILD.gn"""
    filepath = base_path / "chrome/browser/ui/BUILD.gn"

    if not filepath.exists():
        print(f"❌ File not found: {filepath}")
        return False

    backup_file(filepath)

    with open(filepath, 'r') as f:
        content = f.read()

    if '"//chrome/browser/ui/webui/side_panel/datasipper"' in content:
        print("⚠ DataSipper already in UI BUILD.gn")
        return True

    # Find deps section for webui
    marker = 'deps = ['
    insertion = '''  if (enable_datasipper) {
    deps += [ "//chrome/browser/ui/webui/side_panel/datasipper" ]
  }

'''

    # Find the first deps = [ after a "source_set" or "static_library"
    idx = content.find('source_set("ui")')
    if idx != -1:
        deps_idx = content.find('deps = [', idx)
        if deps_idx != -1:
            # Find the closing ] for this deps
            closing_idx = content.find('\n  ]', deps_idx)
            if closing_idx != -1:
                content = content[:closing_idx] + '\n\n' + insertion.rstrip() + content[closing_idx:]
                print("✓ Added DataSipper to UI BUILD.gn")
            else:
                print("⚠ Could not find deps closing bracket")
        else:
            print("⚠ Could not find deps in ui source_set")
    else:
        print("⚠ Could not find ui source_set")

    with open(filepath, 'w') as f:
        f.write(content)

    return True

def register_resources(base_path):
    """Register resources in browser_resources.grd"""
    filepath = base_path / "chrome/browser/browser_resources.grd"

    if not filepath.exists():
        print(f"❌ File not found: {filepath}")
        return False

    backup_file(filepath)

    with open(filepath, 'r') as f:
        content = f.read()

    if 'IDR_DATASIPPER_DATASIPPER_HTML' in content:
        print("⚠ DataSipper resources already registered")
        return True

    # Find a good insertion point (after other side panel resources)
    marker = '</grit-part>'
    resource_xml = '''  <if expr="enable_datasipper">
    <include name="IDR_DATASIPPER_DATASIPPER_HTML"
             file="resources/side_panel/datasipper/datasipper.html"
             type="BINDATA" compress="gzip"/>
    <include name="IDR_DATASIPPER_DATASIPPER_CSS"
             file="resources/side_panel/datasipper/datasipper.css"
             type="BINDATA" compress="gzip"/>
    <include name="IDR_DATASIPPER_DATASIPPER_JS"
             file="resources/side_panel/datasipper/datasipper.js"
             type="BINDATA" compress="gzip"/>
  </if>

'''

    # Find the last </grit-part> before </grit>
    idx = content.rfind(marker, 0, content.rfind('</grit>'))
    if idx != -1:
        content = content[:idx] + resource_xml + content[idx:]
        print("✓ Registered DataSipper resources")
    else:
        print("⚠ Could not find insertion point in GRD file")

    with open(filepath, 'w') as f:
        f.write(content)

    return True

def main():
    print("=" * 60)
    print("DataSipper WebUI Registration Script")
    print("=" * 60)

    # Check we're in the right directory
    if not Path('chrome/browser/ui').exists():
        print("\n❌ Error: Must run from chromium-src/src/ directory")
        print("Current directory:", os.getcwd())
        sys.exit(1)

    base_path = Path('.')

    print("\nStarting registration...")
    print()

    success = True
    success &= register_in_factory(base_path)
    success &= add_to_ui_build(base_path)
    success &= register_resources(base_path)

    print()
    print("=" * 60)
    if success:
        print("✅ Registration complete!")
        print()
        print("Next steps:")
        print("  1. cd chromium-src/src")
        print("  2. gn gen out/DataSipper  # Regenerate build files")
        print("  3. ninja -C out/DataSipper chrome")
        print("  4. ./out/DataSipper/chrome --user-data-dir=/tmp/test --disable-gpu")
        print("  5. Open chrome://datasipper/")
        print()
        print("Backups created with .backup extension")
    else:
        print("⚠ Some registrations failed - check messages above")
        print("Backups were created, you can restore them if needed")
    print("=" * 60)

if __name__ == '__main__':
    main()
