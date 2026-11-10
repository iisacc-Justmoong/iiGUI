"""Build relocated iiGUI consumers without LVUI, LVRS or Qt; never open a window."""
from pathlib import Path, PureWindowsPath
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile


def safe_install_dir(value):
    path = Path(value)
    windows = PureWindowsPath(value)
    if (not value or path.is_absolute() or windows.drive or windows.root or
            ".." in path.parts or ".." in windows.parts):
        raise ValueError("Install directory must stay inside the test prefix: " + value)
    return path


def redirect_install_manifest(script, build, scratch):
    for name in ("install_local_manifest.txt", "${CMAKE_INSTALL_MANIFEST}"):
        old = build.resolve().as_posix() + "/" + name
        new = scratch.resolve().as_posix() + "/" + name
        if script.count(old) != 1 or any(char in new for char in '";\n\r'):
            raise ValueError("Cannot safely redirect the generated install manifest")
        script = script.replace(old, new)
    return script


def forbidden_direct_library(path):
    name = path.replace("\\", "/").rsplit("/", 1)[-1].casefold()
    return name.startswith(("qt", "libqt", "qml", "libqml", "lvui", "liblvui", "lvrs", "liblvrs"))


def cache_values(build):
    text = (build / "CMakeCache.txt").read_text()
    return dict(re.findall(r"^([^/#][^:\n]*):[^=\n]*=(.*)$", text, re.M))


def clean_environment(scratch):
    env = dict(os.environ)
    for key in list(env):
        if key in {"DESTDIR", "CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH",
                   "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH"} or key.startswith(
                       ("LVRS_", "LVUI_", "QT_", "QML", "DYLD_", "LD_")):
            env.pop(key, None)
    env["PATH"] = os.pathsep.join(part for part in env.get("PATH", "").split(os.pathsep)
                                  if not any(name in part.casefold() for name in ("qt", "lvui", "lvrs")))
    for key in ("TMPDIR", "TMP", "TEMP"):
        env[key] = str(scratch)
    return env


def run(command, env, expect_success=True):
    completed = subprocess.run([str(x) for x in command], env=env, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=300)
    if expect_success and completed.returncode:
        raise RuntimeError("Command failed: " + repr(command) + "\n" + completed.stdout[-10000:])
    return completed


def executable(build, name, config):
    for base in (build, build / config):
        for candidate in (base / (name + ".app") / "Contents/MacOS" / name,
                          base / name, base / (name + ".exe")):
            if candidate.is_file():
                return candidate
    raise RuntimeError("Consumer executable was not built: " + name)


def verify_direct_links(exe, env):
    if platform.system() != "Darwin":
        return False
    result = run(["otool", "-L", exe], env)
    lines = result.stdout.splitlines()
    if len(lines) < 2:
        raise RuntimeError("No direct dynamic dependencies were reported for: " + str(exe))
    for line in lines[1:]:
        path = line.split(" (compatibility version", 1)[0].strip()
        if forbidden_direct_library(path):
            raise RuntimeError("Consumer directly links a forbidden library: " + path)
    return True


def verify(root, build, cmake, configuration):
    root, build = root.resolve(), build.resolve()
    if build != root / "build" and root / "build" not in build.parents:
        raise ValueError("Consumer build must remain inside iiGUI/build")
    cache = cache_values(build)
    for key in ("BINDIR", "DATADIR", "DATAROOTDIR", "INCLUDEDIR", "LIBDIR"):
        value = cache.get("CMAKE_INSTALL_" + key, "")
        if value:
            safe_install_dir(value)
    libdir = safe_install_dir(cache.get("CMAKE_INSTALL_LIBDIR") or "lib")
    core_value = cache.get("iiSDKCore_DIR", "")
    core = Path(core_value)
    if not core_value or not core.is_absolute() or not core.is_dir():
        raise ValueError("A configured iiSDKCore package is required for this consumer")
    config = configuration or cache.get("CMAKE_BUILD_TYPE") or "Release"
    generator = cache.get("CMAKE_GENERATOR")
    if not generator:
        raise ValueError("The producer CMake generator is unknown")
    resources = root / "resources"
    with tempfile.TemporaryDirectory(prefix=".independent-consumer-", dir=resources) as resource_tmp, \
         tempfile.TemporaryDirectory(prefix=".independent-consumer-", dir=build) as build_tmp:
        scratch = Path(resource_tmp)
        output = Path(build_tmp)
        env = clean_environment(scratch)
        script = redirect_install_manifest((build / "cmake_install.cmake").read_text(), build, scratch)
        install = scratch / "cmake_install.cmake"
        install.write_text(script)
        stage = output / "stage"
        run([cmake, "-DCMAKE_INSTALL_PREFIX=" + str(stage),
             "-DCMAKE_INSTALL_CONFIG_NAME=" + config, "-P", install], env)
        relocated = output / "relocated"
        stage.rename(relocated)
        package = relocated / libdir / "cmake/iiGUI"
        if not (package / "iiGUIConfig.cmake").is_file():
            raise RuntimeError("The installed iiGUI package was not found")
        options = ["-G", generator, "-DCMAKE_BUILD_TYPE=" + config, "-DBUILD_TESTING=OFF",
                   "-DiiGUI_DIR=" + str(package), "-DiiSDKCore_DIR=" + str(core),
                   "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"]
        for name in ("Qt", "Qt5", "Qt6", "QML", "LVRS", "LVUI"):
            options.append("-DCMAKE_DISABLE_FIND_PACKAGE_" + name + "=ON")
        for name in ("CMAKE_MAKE_PROGRAM", "CMAKE_CXX_COMPILER", "CMAKE_OSX_ARCHITECTURES",
                     "CMAKE_OSX_DEPLOYMENT_TARGET", "Vulkan_INCLUDE_DIR", "Vulkan_LIBRARY"):
            value = cache.get(name)
            if value and not value.endswith("-NOTFOUND"):
                options.append("-D" + name + "=" + value)
        checked_links = []
        for source, name in ((root / "examples/HelloGUI", "HelloGUI"),
                             (root / "tests/standalone_host", "iiGUIStandaloneHost")):
            target = output / name
            run([cmake, "-S", source, "-B", target, *options], env)
            run([cmake, "--build", target, "--config", config, "--target", name, "-j", "2"], env)
            checked_links.append(verify_direct_links(executable(target, name, config), env))
        package_config = package / "iiGUIConfig.cmake"
        package_config.write_text(package_config.read_text() + "\nfind_dependency(LVUI CONFIG REQUIRED)\n")
        rejected = run([cmake, "-S", root / "tests/standalone_host", "-B", output / "negative", *options],
                       env, expect_success=False)
        if not rejected.returncode or "LVUI" not in rejected.stdout or "CMAKE_DISABLE_FIND_PACKAGE_LVUI" not in rejected.stdout:
            raise RuntimeError("The injected required LVUI dependency was not rejected:\n" + rejected.stdout[-6000:])
        print("Relocated iiGUI consumers compiled and linked without LVUI/LVRS/Qt; "
              "injected LVUI dependency rejected; direct links checked=" + str(all(checked_links)).lower() +
              "; GUI applications were not executed.")


if __name__ == "__main__":
    if len(sys.argv) != 5:
        raise SystemExit("usage: independent_consumer.py ROOT BUILD CMAKE CONFIG")
    verify(Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], sys.argv[4])
