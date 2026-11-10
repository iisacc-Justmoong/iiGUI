#!/usr/bin/env python3
"""Compile the embedded D3D HLSL with an optional offline validator.

This checks the HLSL frontend and SPIR-V validity. It does not replace the
Windows D3DCompile shader-model-5 compiler or execution on a D3D11 device.
"""
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]).resolve()
configured = os.environ.get("IIGUI_GLSLANG_VALIDATOR")
compiler = shutil.which(configured or "glslangValidator")
if not compiler:
    if configured:
        raise SystemExit(f"IIGUI_GLSLANG_VALIDATOR does not exist: {configured}")
    print("glslangValidator is unavailable; offline HLSL check skipped")
    raise SystemExit(77)

source = (root / "src/Graphics/CanvasWindows.cpp").read_text(encoding="utf-8")
shader = source.split('R"hlsl(', 1)[1].split(')hlsl"', 1)[0]
entries = (("vert", "vertex_main"), ("frag", "fragment_main"),
           ("vert", "text_vertex"), ("frag", "text_fragment"),
           ("frag", "effect_fragment"))
with tempfile.TemporaryDirectory(prefix=".direct3d-shader-", dir=root / "resources") as directory:
    temporary = Path(directory)
    hlsl = temporary / "canvas.hlsl"
    hlsl.write_text(shader, encoding="utf-8")
    for stage, entry in entries:
        output = temporary / f"{entry}.spv"
        subprocess.run([compiler, "-D", "-V", "--auto-map-bindings", "--spirv-val",
                        "-S", stage, "-e", entry, str(hlsl),
                        "-o", str(output)], check=True)
        if entry != "effect_fragment":
            continue
        # Reflect the compiled module so the texture slots agree with the
        # renderer's five-resource binding contract, including both scenes.
        data = output.read_bytes()
        words = struct.unpack(f"<{len(data) // 4}I", data)
        names, bindings = {}, {}
        index = 5
        while index < len(words):
            length, opcode = words[index] >> 16, words[index] & 0xffff
            if length == 0 or index + length > len(words):
                raise RuntimeError("Malformed SPIR-V instruction")
            if opcode == 5:  # OpName
                raw = struct.pack(f"<{length - 2}I", *words[index + 2:index + length])
                names[words[index + 1]] = raw.split(b"\0", 1)[0].decode("utf-8")
            elif opcode == 71 and length >= 4 and words[index + 2] == 33:  # Binding
                bindings[words[index + 1]] = words[index + 3]
            index += length
        reflected = {name: bindings.get(identifier) for identifier, name in names.items()}
        expected = {"glyphs": 1, "shape_image": 2, "underlay_image": 3,
                    "background_scene": 4, "content_scene": 5}
        if any(reflected.get(name) != binding for name, binding in expected.items()):
            raise RuntimeError(f"D3D material texture bindings changed: {reflected}")
print("D3D HLSL entry points compiled and SPIR-V validated offline")
