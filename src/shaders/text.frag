#version 450
layout(set=0,binding=1) uniform sampler2D glyphs;
layout(location=0) out vec4 output_color;
layout(push_constant) uniform Parameters {vec4 view;vec4 region;} parameters;
void main() {
    ivec2 pixel=ivec2(gl_FragCoord.xy-parameters.region.xy);if(any(lessThan(pixel,ivec2(0)))||any(greaterThanEqual(pixel,textureSize(glyphs,0)))){output_color=vec4(0);return;}
    vec4 color=texelFetch(glyphs,pixel,0);
    output_color=vec4(color.rgb*color.a,color.a);
}
