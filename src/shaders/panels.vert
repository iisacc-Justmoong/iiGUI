#version 450
struct Panel { vec4 bounds; vec4 top; vec4 bottom; vec4 rounding; };
layout(set=0,binding=0,std430) readonly buffer Panels { Panel panels[]; };
layout(push_constant) uniform View { vec2 size; } view;
layout(location=0) out vec2 local;
layout(location=1) out vec2 panel_size;
layout(location=2) out vec4 top;
layout(location=3) out vec4 bottom;
layout(location=4) out float radius;
layout(location=5) out float stroke_width;
void main() {
    const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
    Panel p=panels[gl_InstanceIndex]; vec2 uv=corners[gl_VertexIndex];
    vec2 xy=p.bounds.xy+uv*p.bounds.zw;
    gl_Position=vec4(xy/view.size*2.0-1.0,0,1);
    local=uv*p.bounds.zw; panel_size=p.bounds.zw; top=p.top; bottom=p.bottom; radius=p.rounding.x;stroke_width=p.rounding.y;
}
