#version 450
layout(location=0) in vec2 local;
layout(location=1) in vec2 panel_size;
layout(location=2) in vec4 top;
layout(location=3) in vec4 bottom;
layout(location=4) in float radius;
layout(location=5) in float stroke_width;
layout(location=0) out vec4 output_color;
void main() {
    vec2 q=abs(local-panel_size*0.5)-(panel_size*0.5-radius);
    float d=length(max(q,vec2(0)))+min(max(q.x,q.y),0.0)-radius;
    float coverage=clamp(0.5-d/max(fwidth(d),0.0001),0.0,1.0);
    if(stroke_width>0.0){float r=max(0.0,radius-stroke_width);vec2 qi=abs(local-panel_size*.5)-(panel_size*.5-stroke_width-r);float di=length(max(qi,vec2(0)))+min(max(qi.x,qi.y),0.0)-r;coverage=max(0.0,coverage-clamp(.5-di/max(fwidth(di),.0001),0.0,1.0));}
    vec4 color=mix(top,bottom,clamp(local.y/panel_size.y,0.0,1.0));
    color.a*=coverage; output_color=vec4(color.rgb*color.a,color.a);
}
