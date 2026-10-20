"""Check that distributed SPIR-V names the exact GLSL source snapshot."""
from pathlib import Path
import hashlib
import re
import sys

root=Path(sys.argv[1])
header=(root/'src/VulkanShaders.h').read_text()
for name in ('panels.vert','panels.frag','text.vert','text.frag'):
    source=root/'src/shaders'/name
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    assert f'// {name} SHA256 {digest}' in header, f'{name}: regenerate VulkanShaders.h'
    declaration=re.search(r'inline constexpr std::uint32_t '+name.replace('.','_')+r'\[\]\{\s*(0x[0-9a-f]+)u',header)
    assert declaration and int(declaration.group(1),16)==0x07230203, f'{name}: missing SPIR-V module'
print('Four Vulkan SPIR-V modules match their GLSL source hashes')
