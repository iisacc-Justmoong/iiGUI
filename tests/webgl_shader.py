#!/usr/bin/env python3
"""Compile/link the actual WebGL2 GLSL ES shaders without a browser or GPU."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]).resolve()
configured = os.environ.get("IIGUI_GLSLANG_VALIDATOR")
compiler = shutil.which(configured or "glslangValidator")
if not compiler:
    if configured:
        raise SystemExit(f"IIGUI_GLSLANG_VALIDATOR does not exist: {configured}")
    print("glslangValidator is unavailable; WebGL shader check skipped")
    raise SystemExit(77)

source = (root / "src/Graphics/CanvasWeb.cpp").read_text(encoding="utf-8")
stages = {"panel_vertex": "vert", "panel_fragment": "frag", "quad_vertex": "vert",
          "ink_vertex": "vert", "ink_fragment": "frag", "output_fragment": "frag",
          "effect_fragment": "frag"}
pairs = (("panel_vertex", "panel_fragment"), ("ink_vertex", "ink_fragment"),
         ("quad_vertex", "output_fragment"), ("quad_vertex", "effect_fragment"))
with tempfile.TemporaryDirectory(prefix=".webgl-shader-", dir=root / "resources") as directory:
    paths = {}
    for name, stage in stages.items():
        match = re.search(r'constexpr const char\* ' + name + r'=R"\((.*?)\)";', source, re.S)
        if not match:
            raise RuntimeError(f"Cannot find embedded WebGL shader: {name}")
        paths[name] = Path(directory) / f"{name}.{stage}"
        paths[name].write_text(match.group(1), encoding="utf-8")
    for vertex, fragment in pairs:
        subprocess.run([compiler, "-l", str(paths[vertex]), str(paths[fragment])], check=True)
print("WebGL2 GLSL ES shader pairs compiled and linked offline")
