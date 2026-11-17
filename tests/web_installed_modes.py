"""Verify matching and mismatched installed Web SDK threading contracts without launching an app."""
import argparse
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
for name in ('single-gui', 'threaded-gui', 'single-core', 'threaded-core', 'toolchain'):
    parser.add_argument('--' + name, required=True, type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='.web-mode-', dir=root / 'resources') as inputs, \
     tempfile.TemporaryDirectory(prefix='web-mode-', dir=root / 'build') as outputs:
    source = Path(inputs)
    (source / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.31)
project(WebPackageMode LANGUAGES CXX)
find_package(iiGUI CONFIG REQUIRED)
''')
    for name, gui, core, valid in (
            ('single', args.single_gui, args.single_core, True),
            ('threaded', args.threaded_gui, args.threaded_core, True),
            ('single-with-threaded-core', args.single_gui, args.threaded_core, False),
            ('threaded-with-single-core', args.threaded_gui, args.single_core, False)):
        result = subprocess.run(['cmake', '-S', str(source), '-B', str(Path(outputs) / name), '-G', 'Ninja',
                                 f'-DCMAKE_TOOLCHAIN_FILE={args.toolchain.resolve()}',
                                 f'-DiiGUI_DIR={gui.resolve() / "lib/cmake/iiGUI"}',
                                 f'-DiiSDKCore_DIR={core.resolve() / "lib/cmake/iiSDKCore"}',
                                 '-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH'],
                                capture_output=True, text=True, timeout=60)
        log = result.stdout + result.stderr
        if valid and result.returncode:
            raise AssertionError(f'{name} should configure: {log}')
        if not valid and (not result.returncode or 'Web Core Runtime thread mode does not match iiGUI package' not in log):
            raise AssertionError(f'{name} should reject the incompatible package: {log}')
print('Installed Web SDKs accept matching modes and reject both mismatches')
