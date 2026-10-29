#!/usr/bin/env python3
"""Relocate a complete built SDK prefix and compile installed host consumers.

Does not launch a GUI, browser or device application. Outputs remain under build/.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", required=True, type=Path)
    parser.add_argument("--backend", required=True, choices=("cocoa", "win32", "x11", "wayland", "web"))
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--host-compiler", required=True, type=Path)
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--cache", action="append", default=[], metavar="KEY=VALUE")
    parser.add_argument("--run-contract", action="store_true", help="Run only the headless installed contract on the current native OS")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    output = args.build_root.resolve()
    if not output.is_relative_to(source / "build") or output == source / "build":
        parser.error("Consumer output must be a subdirectory of iiGUI/build")
    if output.exists():
        parser.error("Use a fresh consumer output directory; existing evidence is preserved")
    prefix = args.prefix.resolve(strict=True)
    compiler = args.host_compiler.resolve(strict=True)
    if not (prefix / "lib/cmake/iiGUI/iiGUIConfig.cmake").is_file():
        parser.error("A complete installed target SDK prefix is required")
    if args.run_contract and (args.toolchain or args.backend == "web"):
        parser.error("Headless execution is only supported for the current native OS")
    output.mkdir(parents=True)
    relocated = output / "prefix"
    shutil.copytree(prefix, relocated, symlinks=True)
    example = output / "example"
    shutil.copytree(relocated / "share/iiGUI/examples/HelloHosts", example)
    contract = output / "contract"
    shutil.copytree(source / "tests/host_consumer", contract)
    report = {"backend": args.backend, "relocatedPrefix": str(relocated), "guiLaunched": False,
              "physicalDeviceTested": False, "installedConsumerExecuted": False, "commands": []}
    env = dict(os.environ)
    for key in ("CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH", "PKG_CONFIG_PATH", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS"):
        env.pop(key, None)

    def run(label, command):
        log = output / f"{label}.log"
        with log.open("w") as stream:
            result = subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT)
        report["commands"].append({"step": label, "command": command, "exitCode": result.returncode, "log": str(log)})
        if result.returncode:
            raise RuntimeError(f"{label} failed; inspect {log}")

    common = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DBUILD_TESTING=OFF", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
              "-DCMAKE_CXX_SCAN_FOR_MODULES=OFF",
              f"-DCMAKE_PREFIX_PATH={relocated}", f"-DLVUI_HOST_COMPILER={compiler}",
              "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH", "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF",
              "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF", "-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON", "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON"]
    if args.toolchain:
        common.append(f"-DCMAKE_TOOLCHAIN_FILE={args.toolchain.resolve()}")
    for item in args.cache:
        if "=" not in item:
            parser.error("--cache requires KEY=VALUE")
        common.append("-D" + item)
    try:
        run("example-configure", ["cmake", "-S", str(example), "-B", str(example / "build"), *common])
        run("example-build", ["cmake", "--build", str(example / "build"), "-j", "4", "--target", "HelloHosts"])
        run("example-install", ["cmake", "--install", str(example / "build"), "--prefix", str(output / "application")])
        run("contract-configure", ["cmake", "-S", str(contract), "-B", str(contract / "build"), *common, "-DBUILD_TESTING=ON", f"-DEXPECT_HOST_BACKEND={args.backend}"])
        run("contract-build", ["cmake", "--build", str(contract / "build"), "-j", "4"])
        if args.run_contract:
            run("contract-test", ["ctest", "--test-dir", str(contract / "build"), "--output-on-failure", "--no-tests=error"])
            report["installedConsumerExecuted"] = True
        builds = [example / "build", contract / "build"]
        for build in builds:
            commands = json.loads((build / "compile_commands.json").read_text())
            if any(str(prefix / "include") in command["command"] for command in commands):
                raise RuntimeError("Consumer retained original SDK include path after relocation")
        binary = example / "build" / ({"web": "HelloHosts.wasm", "win32": "HelloHosts.exe", "cocoa": "HelloHosts.app/Contents/MacOS/HelloHosts"}.get(args.backend, "HelloHosts"))
        image = binary.read_bytes()
        if args.backend == "web" and image[:8] != b"\0asm\x01\0\0\0":
            raise RuntimeError("Installed Web consumer did not produce an AOT Wasm module")
        if args.backend in ("wayland", "x11") and image[:4] != b"\x7fELF":
            raise RuntimeError("Installed Linux consumer did not produce an ELF image")
        if args.backend == "win32" and image[:2] != b"MZ":
            raise RuntimeError("Installed Windows consumer did not produce a PE image")
        report.update(status="installed_consumers_built", installedConsumerBuilt=True,
                      binary=str(binary), binarySha256=hashlib.sha256(image).hexdigest(),
                      verifiedAt=datetime.datetime.now(datetime.timezone.utc).isoformat())
    except Exception as error:
        report.update(status="failed", error=str(error))
        raise
    finally:
        (output / "verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(output / "verification.json")


if __name__ == "__main__":
    main()
