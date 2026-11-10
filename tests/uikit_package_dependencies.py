"""Check relocated iiGUI dependency lookup from an iPhoneOS cross configuration."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile


def package(parent, name, origin):
    folder = parent / name
    folder.mkdir(parents=True)
    (folder / f"{name}Config.cmake").write_text(f'set({name}_FOUND TRUE)\nset({name}_ORIGIN "{origin}")\n')
    (folder / f"{name}ConfigVersion.cmake").write_text(
        'set(PACKAGE_VERSION "3.5.0")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\nset(PACKAGE_VERSION_EXACT TRUE)\n')
    return folder


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--vulkan-include", type=Path, required=True)
    parser.add_argument("--vulkan-library", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"} or key.startswith(("QT_", "QML", "LVRS_", "DYLD_", "LD_")):
            env.pop(key, None)
    sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True, env=env).strip()
    with tempfile.TemporaryDirectory(prefix=".ios-package-input-", dir=root / "resources") as resource, \
         tempfile.TemporaryDirectory(prefix="ios-package-build-", dir=args.build) as output:
        resource, output = Path(resource), Path(output)
        packages = resource / "relocated/lib/cmake"
        gui = packages / "iiGUI"
        gui.mkdir(parents=True)
        shutil.copy2(args.build / "iiGUIConfig.cmake", gui / "iiGUIConfig.cmake")
        (gui / "iiGUITargets.cmake").write_text("add_library(iiGUI::iiGUI INTERFACE IMPORTED)\n")
        package(packages, "plutovg", "bundled")
        package(packages, "lunasvg", "bundled")
        custom = resource / "override"
        override_pluto = package(custom, "plutovg", "override")
        override_luna = package(custom, "lunasvg", "override")
        core = resource / "core"
        core.mkdir()
        (core / "iiSDKCoreConfig.cmake").write_text("add_library(iiSDKCore::Runtime INTERFACE IMPORTED)\nset(iiSDKCore_Runtime_FOUND TRUE)\n")
        (core / "iiSDKCoreConfigVersion.cmake").write_text('set(PACKAGE_VERSION "0.20.0")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n')
        source = resource / "consumer"
        source.mkdir()
        (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.31)
project(IIGUIIOSPackageProbe LANGUAGES CXX)
find_package(iiGUI CONFIG REQUIRED)
if(NOT plutovg_ORIGIN STREQUAL EXPECTED_ORIGIN OR NOT lunasvg_ORIGIN STREQUAL EXPECTED_ORIGIN)
    message(FATAL_ERROR "Unexpected SVG package origin: ${plutovg_ORIGIN};${lunasvg_ORIGIN}")
endif()
''')
        for name in ("bundled", "override"):
            command = ["cmake", "-S", str(source), "-B", str(output / name / "build"), "-G", "Ninja",
                       "-DCMAKE_SYSTEM_NAME=iOS", f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_OSX_ARCHITECTURES=arm64",
                       "-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0", f"-DiiGUI_DIR={gui}", f"-DiiSDKCore_DIR={core}",
                       f"-DVulkan_INCLUDE_DIR={args.vulkan_include}", f"-DVulkan_LIBRARY={args.vulkan_library}",
                       f"-DEXPECTED_ORIGIN={name}"]
            if name == "override":
                command += [f"-Dplutovg_DIR={override_pluto}", f"-Dlunasvg_DIR={override_luna}"]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
            if result.returncode:
                raise RuntimeError(f"{name} cross-package lookup failed:\n" + result.stdout)
    print("Relocated and explicitly overridden iPhoneOS package dependencies configured; no app executed")


if __name__ == "__main__":
    main()
