"""Confirm private/test resources never enter an installed SDK package."""
from pathlib import Path, PureWindowsPath
import os
import re
import shutil
import subprocess
import sys
import uuid

provider_dir = os.environ.get("IIFILEPROVIDER_PYTHON_DIRECTORY")
sys.path.insert(0, provider_dir or str(Path.home() / ".local/SDK/iiFileProvider/share/iiFileProvider/python"))
import ii_file_provider as provider
root = Path(__file__).resolve().parents[1]
build, cmake, datadir = Path(sys.argv[1]), sys.argv[2], Path(sys.argv[3])
cache = (build / "CMakeCache.txt").read_text()
for name in ("BINDIR", "DATADIR", "DATAROOTDIR", "INCLUDEDIR", "LIBDIR"):
    match = re.search(r"^CMAKE_INSTALL_" + name + r":[^=]*=(.*)$", cache, re.M)
    if match and match[1] and (Path(match[1]).is_absolute() or PureWindowsPath(match[1]).is_absolute()
                               or ".." in Path(match[1]).parts or ".." in PureWindowsPath(match[1]).parts):
        raise RuntimeError("Refusing an install test with an absolute or escaping install directory: " + name)
if (datadir.is_absolute() or PureWindowsPath(str(datadir)).is_absolute() or ".." in datadir.parts
        or ".." in PureWindowsPath(str(datadir)).parts):
    raise RuntimeError("Installed resource directory must be relative")
owner = root / "resources/.state" / ("install-" + uuid.uuid4().hex[:16])
provider.mkdir(owner, parents=True, exist_ok=False)
try:
    stage = owner / "prefix"
    env = {key: value for key, value in os.environ.items() if key.upper() != "DESTDIR"}
    # CMake's generated top-level install script otherwise rewrites the real
    # build's install manifest, even when the prefix points at a test fixture.
    # Redirect both variants in a private copy and install only this directory.
    source = (build / "cmake_install.cmake").read_text()
    for name in ("install_local_manifest.txt", "${CMAKE_INSTALL_MANIFEST}"):
        original = build.resolve().as_posix() + "/" + name
        if source.count(original) != 1:
            raise RuntimeError("Cannot safely redirect the generated install manifest")
        source = source.replace(original, (owner / name).as_posix())
    script = owner / "cmake_install.cmake"
    provider.write_text(script, source)
    manifest = build / "install_manifest.txt"
    prior_manifest = manifest.read_bytes() if manifest.is_file() else None
    result = subprocess.run([cmake, "-DCMAKE_INSTALL_LOCAL_ONLY=ON",
                             "-DCMAKE_INSTALL_PREFIX=" + str(stage), "-P", str(script)],
                            capture_output=True, text=True, env=env)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    resources = stage / datadir / "iiGUI/resources"
    assert (resources / "native/Pretendard-Regular.ttf").is_file()
    for name in (".state", ".recovery", "dependencies", "font-metadata"):
        assert not (resources / name).exists(), "Private or test resources installed: " + name
    assert not any(path.name.startswith(".") for path in resources.rglob("*")), "Hidden resource installed"
    assert (manifest.read_bytes() if manifest.is_file() else None) == prior_manifest, "Build install manifest changed"
finally:
    shutil.rmtree(owner)
