#!/usr/bin/env python3
"""Compile and audit native AOT consumers; deliberately never launch a GUI."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

from verify_desktop import build_environment, checked_build_root, host_platform

RESOURCES = tuple(entry['file'] for entry in json.loads((Path(__file__).resolve().parents[1]/'resources/lvrs-native-appearance.json').read_text())['fonts'])+("Pretendard-OFL.txt","Inter-OFL.txt")


def make_manifest(source, output, platform, compiler, toolchain=None, cache=None):
    data = json.loads((source / "examples/HelloWorld/iiSDKBuilder.json").read_text(encoding="utf-8"))
    data["app"]["source"] = (source / "examples/HelloWorld").as_posix()
    data["sdkRoot"] = source.parent.as_posix()
    data["buildRoot"] = output.as_posix()
    for package in data["sdks"]:
        package["source"] = (source.parent / package["name"]).as_posix()
    data["kits"] = [kit for kit in data["kits"] if kit["platform"] == platform]
    if len(data["kits"]) != 1:
        raise ValueError("Expected one native desktop kit")
    kit = data["kits"][0]
    kit["cache"].update(cache or {})
    # Always execute a host compiler, including in a relocated cross package.
    kit["cache"]["LVUI_HOST_COMPILER"] = compiler.resolve().as_posix()
    kit["cache"]["CMAKE_EXPORT_COMPILE_COMMANDS"] = "ON"
    kit["cache"]["CMAKE_POSITION_INDEPENDENT_CODE"] = "ON"
    if toolchain:
        kit["toolchain"] = toolchain.resolve().as_posix()
    return data


def native_image(data, platform):
    if platform == "windows":
        if len(data) < 64 or data[:2] != b"MZ":
            raise ValueError("Expected a Windows PE executable")
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        if pe + 96 > len(data) or data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("Invalid PE header")
        machine = struct.unpack_from("<H", data, pe + 4)[0]
        magic = struct.unpack_from("<H", data, pe + 24)[0]
        subsystem = struct.unpack_from("<H", data, pe + 24 + 68)[0]
        if machine not in (0x8664, 0xaa64) or magic != 0x20b or subsystem != 2:
            raise ValueError("Expected a 64-bit native Windows GUI image")
        return {"format": "PE32+", "architecture": {0x8664: "x86_64", 0xaa64: "aarch64"}[machine], "subsystem": "Windows GUI"}
    if platform == "linux":
        if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
            raise ValueError("Expected a little-endian ELF64 executable")
        kind, machine = struct.unpack_from("<HH", data, 16)
        if kind not in (2, 3) or machine not in (62, 183):
            raise ValueError("Unsupported Linux native executable architecture/type")
        return {"format": "ELF64", "architecture": {62: "x86_64", 183: "aarch64"}[machine]}
    if platform == "macos" and len(data) >= 32:
        magic, cpu, _, kind = struct.unpack_from("<IIII", data)
        if magic == 0xfeedfacf and kind == 2 and cpu in (0x100000c, 0x1000007):
            return {"format": "Mach-O64", "architecture": "aarch64" if cpu == 0x100000c else "x86_64"}
    raise ValueError("Expected a supported native executable")


def check_linkage(libraries, symbols):
    if re.search(r"(?:lib)?Qt\d|QtCore|QtGui|QtWidgets|python\d|libpython|libjvm|jvm\.dll|WebKit|QtQml", libraries, re.I):
        raise ValueError("GUI framework or interpreter dependency in native application")
    if any(name in symbols for name in ("lvui::parse(", "lvui::parse_view(", "lvui::compile_native(")):
        raise ValueError("LVUI parser/compiler linked into the native application")
    for required in ("ii::runtime::Application::exec(", "ii::gui::Host::mount_widgets("):
        if required not in symbols:
            raise ValueError("Missing native application contract: " + required)


def check_resources(prefix, app_build, platform):
    destination = app_build / ("HelloWorld.app/Contents/Resources" if platform == "macos" else "resources")
    hashes = {}
    for name in RESOURCES:
        expected = hashlib.sha256((prefix / "share/iiGUI/resources/native" / name).read_bytes()).hexdigest()
        actual = hashlib.sha256((destination / name).read_bytes()).hexdigest()
        if actual != expected:
            raise ValueError("Native resource changed: " + name)
        hashes[name] = actual
    return hashes


def main(argv=None):
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", required=True, choices=("windows", "linux", "macos"))
    parser.add_argument("--builder", default=shutil.which("iisdk-builder") or "iisdk-builder")
    parser.add_argument("--host-compiler", type=Path, required=True)
    parser.add_argument("--toolchain", type=Path)
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--llvm-readobj", default=shutil.which("llvm-readobj") or "llvm-readobj")
    parser.add_argument("--llvm-nm", default=shutil.which("llvm-nm") or "llvm-nm")
    parser.add_argument("--cache", action="append", default=[], metavar="KEY=VALUE")
    args = parser.parse_args(argv)
    try:
        if not args.host_compiler.resolve().is_file():
            raise ValueError("A host-native lvui-compile executable is required")
        if args.platform != host_platform() and not args.toolchain:
            raise ValueError("A foreign desktop target requires --toolchain")
        if args.toolchain and not args.toolchain.resolve().is_file():
            raise ValueError("Toolchain does not exist")
        cache = {}
        for entry in args.cache:
            key, separator, value = entry.partition("=")
            if not key or not separator:
                raise ValueError("--cache requires KEY=VALUE")
            cache[key] = value
        output = checked_build_root(source, args.build_root or source / "build/verify-aot-desktop" / args.platform)
    except ValueError as error:
        parser.error(str(error))
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    run = output / "runs" / stamp
    run.mkdir(parents=True, exist_ok=False)
    manifest = run / "manifest.json"
    data = make_manifest(source, output, args.platform, args.host_compiler, args.toolchain, cache)
    manifest.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    report = {"platform": args.platform, "buildHost": host_platform(), "execution": "not_requested",
              "physicalDeviceTested": False, "guiLaunched": False, "manifest": str(manifest), "commands": []}
    environment = build_environment(args.toolchain)
    for key in ("CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS"):
        environment.pop(key, None)

    def execute(label, command):
        log = run / (label + ".log")
        print(label + ": " + str(log), flush=True)
        with log.open("w", encoding="utf-8") as stream:
            result = subprocess.run([str(x) for x in command], stdout=stream, stderr=subprocess.STDOUT, env=environment)
        report["commands"].append({"step": label, "exitCode": result.returncode, "log": str(log)})
        if result.returncode:
            raise RuntimeError(label + " failed; inspect " + str(log))
        return log.read_text(encoding="utf-8", errors="replace")

    try:
        execute("builder", [args.builder, "build", "--manifest", manifest])
        kit = output / "kits" / args.platform / "Release"
        # Verify the installed contract after moving the complete kit. No authoring
        # headers or CMake build-tree target exports may be used by this consumer.
        prefix = run / "relocated"
        shutil.copytree(kit / "prefix", prefix, symlinks=True)
        consumer = run / "consumer"
        shutil.copytree(prefix / "share/iiGUI/examples/HelloWorld", consumer / "source")
        app_build = consumer / "build"
        flags = ["-D" + key + "=" + value for key, value in data["kits"][0]["cache"].items()]
        flags += ["-D" + package + "_DIR:PATH=" + str(prefix / "lib/cmake" / package)
                  for package in ("iiSDKCore", "iiColorManager", "LVUI", "iiGUI", "lunasvg", "plutovg")]
        if args.toolchain:
            flags += ["-DCMAKE_TOOLCHAIN_FILE=" + str(args.toolchain.resolve())]
        cmake = shutil.which("cmake") or "cmake"
        execute("consumer-configure", [cmake, "-S", consumer / "source", "-B", app_build, "-G", "Ninja",
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=" + str(prefix),
                "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF", *flags])
        execute("consumer-build", [cmake, "--build", app_build, "--parallel", "4"])
        binary = app_build / {"windows": "HelloWorld.exe", "linux": "HelloWorld", "macos": "HelloWorld.app/Contents/MacOS/HelloWorld"}[args.platform]
        image = native_image(binary.read_bytes(), args.platform)
        libraries = execute("dependencies", [args.llvm_readobj, "--file-headers", "--needed-libs", "--coff-imports", binary])
        symbols = execute("symbols", [args.llvm_nm, "--defined-only", "--demangle", binary])
        check_linkage(libraries, symbols)
        commands = (app_build / "compile_commands.json").read_text(encoding="utf-8")
        for package in ("iiGUI", "LVUI", "iiSDKCore", "iiColorManager"):
            if str(source.parent / package / "src") in commands:
                raise ValueError("Installed consumer accessed authoring headers: " + package)
        entry = str(prefix / "lib/cmake/iiSDKCore/ApplicationEntry.cpp")
        if entry not in commands:
            raise ValueError("Consumer did not compile the installed iiSDKCore entrypoint")
        generated = app_build / "HelloWorld.generated.h"
        font_hashes = check_resources(prefix, app_build, args.platform)
        target = "HostWindows.cpp" if args.platform == "windows" else "HostLinux.cpp" if args.platform == "linux" else "Host.mm"
        gui_commands = (kit / "sdk/iiGUI/build/compile_commands.json").read_text(encoding="utf-8")
        if target not in gui_commands:
            raise ValueError("Native platform host was not compiled: " + target)
        execute("consumer-install", [cmake, "--install", app_build, "--prefix", consumer / "install"])
        report.update(status="native_build_and_package_verified", image=image, binary=str(binary),
                      binarySha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                      sourceSha256=hashlib.sha256((consumer / "source/HelloWorld.lvui").read_bytes()).hexdigest(),
                      generatedSha256=hashlib.sha256(generated.read_bytes()).hexdigest(), nativeHostSource=target,
                      qtLinked=False, interpreterLinked=False, lvuiParserLinked=False, coreEntryCompiled=True,
                      installedConsumerBuilt=True, installedConsumerExecuted=False, resources=font_hashes)
        return 0
    except Exception as error:
        report.update(status="failed", error=str(error))
        print(str(error), file=sys.stderr)
        return 1
    finally:
        destination = run / "verification.json"
        destination.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print("Verification report: " + str(destination), flush=True)


if __name__ == "__main__":
    sys.exit(main())
