"""Build a staged installed-package consumer without opening an application."""
import argparse
import os
from pathlib import Path, PureWindowsPath
import re
import subprocess
import tempfile

import resource_workspace

ROOT = Path(__file__).resolve().parents[1]


def run(command, env):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env)
    if result.returncode:
        raise RuntimeError(result.stdout)


def main():
    parser = argparse.ArgumentParser()
    for name in ("cmake", "compiler", "core-cmake", "build-root"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--vulkan-include", default="")
    parser.add_argument("--vulkan-library", default="")
    args = parser.parse_args()
    build = Path(args.build_root).resolve(strict=True)
    cache = (build / "CMakeCache.txt").read_text()
    paths = {}
    for name in ("BINDIR", "DATADIR", "DATAROOTDIR", "INCLUDEDIR", "LIBDIR"):
        found = re.search(r"^CMAKE_INSTALL_" + name + r":[^=]*=(.*)$", cache, re.M)
        if found and found[1] and (Path(found[1]).is_absolute() or PureWindowsPath(found[1]).is_absolute()
                                  or ".." in Path(found[1]).parts or ".." in PureWindowsPath(found[1]).parts):
            raise RuntimeError("Install directory escapes its temporary prefix: " + name)
        if found and found[1]:
            paths[name] = Path(found[1])
    env = {k: v for k, v in os.environ.items() if k.upper() != "DESTDIR"}
    with resource_workspace.temporary(ROOT, "paint-package-") as owner:
        script = (build / "cmake_install.cmake").read_text()
        for name in ("install_local_manifest.txt", "${CMAKE_INSTALL_MANIFEST}"):
            original = build.as_posix() + "/" + name
            if script.count(original) != 1:
                raise RuntimeError("Cannot safely redirect the install manifest")
            script = script.replace(original, (owner / name).as_posix())
        install_script = owner / "cmake_install.cmake"
        resource_workspace.provider.write_text(install_script, script)
        stage = owner / "stage"
        manifest = build / "install_manifest.txt"
        before = manifest.read_bytes() if manifest.is_file() else None
        run([args.cmake, f"-DCMAKE_INSTALL_PREFIX={stage}", "-P", str(install_script)], env)
        if not (stage / paths.get("INCLUDEDIR", Path("include")) / "iiGUI/PaintView.h").is_file():
            raise RuntimeError("PaintView.h was not installed")
        if not (stage / paths.get("LIBDIR", Path("lib")) / "cmake/iiGUI/iiGUILvui.cmake").is_file():
            raise RuntimeError("The installed LVUI helper is missing")
        if not (stage / paths.get("LIBDIR", Path("lib")) / "cmake/iiGUIPainter/iiGUIPainterConfig.cmake").is_file():
            raise RuntimeError("The host-independent painter package is missing")
        with tempfile.TemporaryDirectory(prefix="paint-consumer-", dir=build) as target:
            configure = [args.cmake, "-S", str(ROOT / "tests/lvui_package"), "-B", target,
                         f"-DIIGUI_STAGE={stage}", f"-DIIGUI_TEST_COMPILER={args.compiler}",
                         f"-DiiSDKCore_DIR={args.core_cmake}"]
            if args.vulkan_include:
                configure.append(f"-DVulkan_INCLUDE_DIR={args.vulkan_include}")
            if args.vulkan_library:
                configure.append(f"-DVulkan_LIBRARY={args.vulkan_library}")
            run(configure, env)
            run([args.cmake, "--build", target, "-j", "2"], env)
        with tempfile.TemporaryDirectory(prefix="paint-consumer-", dir=build) as target:
            configure = [args.cmake, "-S", str(ROOT / "tests/painter_package"), "-B", target,
                         f"-DIIGUI_STAGE={stage}", f"-DiiSDKCore_DIR={args.core_cmake}"]
            run(configure, env)
            run([args.cmake, "--build", target, "-j", "2"], env)
        if (manifest.read_bytes() if manifest.is_file() else None) != before:
            raise RuntimeError("The existing build install manifest changed")
    print("Staged iiGUI package and LVUI-generated C++ consumer compiled and linked.")


if __name__ == "__main__":
    main()
