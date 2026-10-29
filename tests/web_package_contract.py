"""Reject an installed Web Core package that omits its required thread contract.

Usage: python3 -B tests/web_package_contract.py --prefix WEB_PREFIX --toolchain EMSCRIPTEN_CMAKE --output build/unique-probe
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--toolchain", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    output = args.output.resolve()
    if not output.is_relative_to(source / "build") or output.exists():
        parser.error("Use a fresh output under iiGUI/build")
    prefix = args.prefix.resolve(strict=True)
    package = output / "incomplete-prefix"
    (package / "lib").mkdir(parents=True)
    shutil.copytree(prefix / "lib/cmake/iiSDKCore", package / "lib/cmake/iiSDKCore")
    for archive in (prefix / "lib").glob("*.a"):
        (package / "lib" / archive.name).symlink_to(archive)
    (package / "include").symlink_to(prefix / "include", target_is_directory=True)
    exports = list((package / "lib/cmake/iiSDKCore").glob("*Targets.cmake"))
    removed = 0
    for export in exports:
        text = export.read_text()
        removed += text.count("-pthread")
        export.write_text(text.replace(";-pthread", "").replace("-pthread;", "").replace("-pthread", ""))
    if not removed:
        raise AssertionError("The positive reference package did not advertise pthread support")
    env = dict(os.environ)
    for key in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH", "PKG_CONFIG_PATH", "CFLAGS", "CXXFLAGS", "LDFLAGS"):
        env.pop(key, None)
    result = subprocess.run(["cmake", "-S", str(source), "-B", str(output / "build"), "-G", "Ninja",
                             f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain.resolve()}", f"-DCMAKE_PREFIX_PATH={package}",
                             "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH", "-DBUILD_TESTING=OFF", "-DIIGUI_BUILD_EXAMPLES=OFF"],
                            env=env, capture_output=True, text=True, timeout=60)
    log = result.stdout + result.stderr
    (output / "configure.log").write_text(log)
    if not result.returncode or "Core Runtime must export -pthread" not in log:
        raise AssertionError("Incompatible Core package was not rejected by the Web host contract: " + log[-1500:])
    print("WEB_INCOMPATIBLE_CORE_REJECTED")


if __name__ == "__main__":
    main()
