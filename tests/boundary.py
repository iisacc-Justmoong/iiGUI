"""Source/package boundary guard; build-host tooling, never an app dependency."""
from pathlib import Path
import json
import re
import sys

root = Path(sys.argv[1])
violations = []
for base in (root / 'include', root / 'src'):
    for file in base.rglob('*'):
        if not file.is_file() or file.suffix not in {'.h', '.hpp', '.cpp', '.cc', '.mm', '.m'}:
            continue
        source = file.read_text()
        if re.search(r'^\s*#\s*(?:include|import)\s*[<"](?:Qt|Q[A-Z]|SDL|WebKit|WebView|webkit|cef_)', source, re.M):
            violations.append(f'{file}: GUI-framework dependency')
        if file.is_relative_to(root / 'include') and re.search(
                r'^\s*#\s*(?:include|import)\s*[<"](?:AppKit|Foundation|CoreFoundation|Metal|QuartzCore|CoreGraphics|objc|[Ww]indows\.h|[Dd]3[dD]|[Dd][Xx][Gg][Ii]|d2d|dwrite|vulkan/|EGL/|GLES|X11/|wayland|sys/)', source, re.M):
            violations.append(f'{file}: native type leaked into public API')
        if re.search(r'\b(?:ofstream|fopen|freopen|creat)\s*\(', source):
            violations.append(f'{file}: file creation bypasses iiFileProvider')
for file in (root / 'CMakeLists.txt', root / 'cmake/iiGUIConfig.cmake.in'):
    if re.search(r'(?:find_package|find_dependency)\s*\(\s*(?:Qt|SDL|WebKit|WebView|CEF)', file.read_text()):
        violations.append(f'{file}: framework package dependency')
manifest = json.loads((root / 'examples/HelloGUI/iiSDKBuilder.json').read_text())
if {kit['platform'] for kit in manifest['kits']} != {'windows', 'macos', 'linux'}:
    violations.append('HelloGUI manifest must include the three native desktop kits')
if 'Python3_EXECUTABLE' in json.dumps(manifest):
    violations.append('Portable HelloGUI manifest must discover host Python, not pin a machine path')
hello = (root / 'examples/HelloGUI/main.cpp').read_text()
if re.search(r'^\s*#\s*(?:if|elif)', hello, re.M):
    violations.append('HelloGUI must use the same C++23 application code on every desktop')
if violations:
    raise SystemExit('\n'.join(violations))
print('iiGUI source/package boundary: C++23 public API, Core/LVUI SDKs, no Qt/SDL/browser/WebView/file creation')
