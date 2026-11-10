"""Verify source reuse and preservation without fetching external code."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import uuid

provider_dir = os.environ.get("IIFILEPROVIDER_PYTHON_DIRECTORY")
sys.path.insert(0, provider_dir or str(Path.home() / ".local/SDK/iiFileProvider/share/iiFileProvider/python"))
import ii_file_provider as provider
root = Path(__file__).resolve().parents[1]
owner = root / "resources/.state" / ("svg-" + uuid.uuid4().hex[:16])
provider.mkdir(owner, parents=True, exist_ok=False)
try:
    source = owner / "resources/dependencies/lunasvg"
    def call(mode):
        return subprocess.run([sys.argv[1], "-DROOT=" + str(owner),
            "-DHELPER=" + str(root / "cmake/SvgSource.cmake"), "-DMODE=" + mode,
            "-P", str(root / "tests/svg_source.cmake")], capture_output=True, text=True)
    assert call("missing").returncode == 0
    provider.mkdir(source, parents=True, exist_ok=False)
    assert call("incomplete").returncode != 0
    assert call("override").returncode == 0
    provider.mkdir(source / "plutovg/source", parents=True, exist_ok=True)
    for path in (source / "CMakeLists.txt", source / "plutovg/CMakeLists.txt",
                 source / "plutovg/source/plutovg-stb-image.h"):
        provider.write_text(path, "fixture")
    assert call("valid").returncode == 0
    assert (source / "CMakeLists.txt").read_text() == "fixture"
finally:
    shutil.rmtree(owner)
