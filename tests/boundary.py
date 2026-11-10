"""Source/package boundary guard; build-host tooling, never an app dependency."""
from pathlib import Path
import json
import re
import sys

from manifest_boundary import hello_manifest_violations
from source_boundary import (forbidden_cmake_dependencies, forbidden_component_rendering,
                             forbidden_source_dependencies)

root = Path(sys.argv[1])
violations = []
sources = {p.name: p for area in ('API', 'Core', 'Graphics', 'Platform', 'Presentation', 'Text', 'Widgets') for p in (root / 'src' / area).iterdir() if p.is_file()}
public_headers = {sources[name] for name in
                  ('Graphics.h', 'Image.h', 'Host.h', 'Text.h', 'NativeWidgets.h', 'ProgressDrawing.h', 'Appearance.h', 'Application.h', 'iiGUI.h')}
for header in public_headers:
    if not header.is_file():
        violations.append(f'{header}: public header missing from source directory')
component_hosts = {root / 'src/Platform' / name for name in
                   ('Host.mm', 'HostIOS.mm', 'HostWindows.cpp', 'HostLinux.cpp', 'HostWayland.cpp',
                    'HostAndroid.cpp', 'HostWeb.cpp')}
for base in (root / 'src',):
    for file in base.rglob('*'):
        if not file.is_file() or file.suffix not in {'.h', '.hpp', '.cpp', '.cc', '.mm', '.m'}:
            continue
        source = file.read_text()
        violations.extend(f'{file}: {issue}' for issue in forbidden_source_dependencies(source))
        if (file in component_hosts or file.is_relative_to(root / 'src/Widgets') or
                file.is_relative_to(root / 'src/Graphics')):
            violations.extend(f'{file}: {issue}' for issue in forbidden_component_rendering(source))
        if file in public_headers and re.search(
                r'^\s*#\s*(?:include|import)\s*[<"](?:AppKit|Foundation|CoreFoundation|Metal|QuartzCore|CoreGraphics|objc|[Ww]indows\.h|[Dd]3[dD]|[Dd][Xx][Gg][Ii]|d2d|dwrite|vulkan/|EGL/|GLES|X11/|wayland|android/|emscripten|sys/)', source, re.M):
            violations.append(f'{file}: native type leaked into public API')
        if re.search(r'\b(?:ofstream|fopen|freopen|creat)\s*\(', source):
            violations.append(f'{file}: file creation bypasses iiFileProvider')
cmake_files = [root / 'CMakeLists.txt'] + sorted(
    file for file in (root / 'cmake').rglob('*')
    if file.is_file() and (file.name.endswith('.cmake') or file.name.endswith('.cmake.in')))
for file in cmake_files:
    violations.extend(f'{file}: {issue}' for issue in forbidden_cmake_dependencies(file.read_text()))
manifest = json.loads((root / 'examples/HelloGUI/iiSDKBuilder.json').read_text())
violations.extend(hello_manifest_violations(manifest, root / 'examples/HelloGUI', root))
if {kit['platform'] for kit in manifest['kits']} != {'windows', 'macos', 'linux'}:
    violations.append('HelloGUI manifest must include the three native desktop kits')
if 'Python3_EXECUTABLE' in json.dumps(manifest):
    violations.append('Portable HelloGUI manifest must discover host Python, not pin a machine path')
hello = (root / 'examples/HelloGUI/main.cpp').read_text()
if re.search(r'^\s*#\s*(?:if|elif)', hello, re.M):
    violations.append('HelloGUI must use the same C++23 application code on every desktop')
if violations:
    raise SystemExit('\n'.join(violations))
print('iiGUI source/package boundary: C++23 public API, no LVUI/LVRS/Qt/SDL/WebView or file-provider bypass')
