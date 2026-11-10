"""Check that distributed SPIR-V names the exact GLSL source snapshot."""
from pathlib import Path
import hashlib
import re
import sys

root=Path(sys.argv[1])
header=(root/'src/Graphics/VulkanShaders.h').read_text()
for name in ('panels.vert','panels.frag','text.vert','text.frag','effect.frag'):
    source=root/'src/shaders'/name
    digest=hashlib.sha256(source.read_bytes()).hexdigest()
    assert f'// {name} SHA256 {digest}' in header, f'{name}: regenerate VulkanShaders.h'
    declaration=re.search(r'inline constexpr std::uint32_t '+name.replace('.','_')+r'\[\]\{\s*(0x[0-9a-f]+)u',header)
    assert declaration and int(declaration.group(1),16)==0x07230203, f'{name}: missing SPIR-V module'
    if name=='effect.frag':
        block=re.search(r'inline constexpr std::uint32_t effect_frag\[\]\{(.*?)\};',header,re.S)
        assert block, 'effect.frag: missing shader words'
        words=[int(value,16) for value in re.findall(r'0x([0-9a-f]+)u',block.group(1))]
        decorations={}
        at=5
        while at<len(words):
            length,opcode=words[at]>>16,words[at]&0xffff
            assert length and at+length<=len(words), 'effect.frag: malformed SPIR-V instruction'
            if opcode==71 and length>=4 and words[at+2] in (33,34):
                decorations.setdefault(words[at+1],{})[words[at+2]]=words[at+3]
            at+=length
        bindings={(item.get(34),item.get(33)) for item in decorations.values()}
        assert {(0,1),(0,2),(0,3),(0,4),(0,5)}<=bindings, 'effect.frag: missing material scene descriptors'
print('Five Vulkan SPIR-V modules match their GLSL source hashes')
