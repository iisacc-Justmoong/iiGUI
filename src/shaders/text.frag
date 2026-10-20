#version 450
layout(set=0,binding=1) uniform sampler2D glyphs;
layout(location=0) out vec4 output_color;
void main() {
    vec4 color=texelFetch(glyphs,ivec2(gl_FragCoord.xy),0);
    output_color=vec4(color.rgb*color.a,color.a);
}
