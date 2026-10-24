#version 450
layout(set=0,binding=1) uniform sampler2D source_image;
layout(set=0,binding=2) uniform sampler2D shape_image;
layout(set=0,binding=3) uniform sampler2D underlay_image;
layout(push_constant) uniform Parameters {vec4 view;vec4 bounds;vec4 tint;vec4 border;vec4 style;vec4 underlay;vec4 base;vec4 highlight;} parameters;
layout(location=0) out vec4 output_color;
bool flag(int n){return (int(parameters.style.w)&n)!=0;}
float distance_to_shape(vec2 point,vec4 bounds,float radius){vec2 q=abs(point-bounds.xy-bounds.zw*.5)-(bounds.zw*.5-radius);return length(max(q,0))+min(max(q.x,q.y),0)-radius;}
float mask(vec2 point){
    if(flag(2)){vec2 uv=(point-parameters.bounds.xy)/parameters.bounds.zw;if(any(lessThan(uv,vec2(0)))||any(greaterThan(uv,vec2(1))))return 0;return texture(shape_image,uv).a;}
    return clamp(.5-distance_to_shape(point,parameters.bounds,parameters.style.x)*parameters.view.z,0,1);
}
vec4 over(vec4 a,vec4 b){return a+b*(1-a.a);}
vec4 input_at(vec2 point){
    vec4 value=vec4(parameters.base.rgb*parameters.base.a,parameters.base.a);
    if(flag(8)){vec4 scene=texture(source_image,point/parameters.view.xy);scene.rgb*=scene.a;value=over(scene,value);}
    if(flag(4)){vec2 uv=(point-parameters.underlay.xy)/parameters.underlay.zw;if(all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)))){vec4 image=texture(underlay_image,uv);if(flag(16)){vec4 tint=vec4(unpackHalf2x16(floatBitsToUint(parameters.highlight.z)),unpackHalf2x16(floatBitsToUint(parameters.highlight.w)));image=vec4(tint.rgb,image.a*tint.a);}image.rgb*=image.a;value=over(image,value);}}
    return value;
}
void main(){
    float scale=parameters.view.z,opacity=parameters.view.w,blur=parameters.style.y;vec2 point=gl_FragCoord.xy/scale;bool shadow=flag(1);
    if(opacity<=0||distance_to_shape(point,parameters.bounds,flag(2)?0:parameters.style.x)>(shadow?blur+1:1)){output_color=vec4(0);return;}
    float coverage=mask(point);vec4 paint;
    if(shadow){if(blur>.01){coverage=0;float total=0,sigma=blur/3,step_size=blur/6;for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x){vec2 offset=vec2(x,y)*step_size;float weight=exp(-dot(offset,offset)/(2*sigma*sigma));coverage+=mask(point+offset)*weight;total+=weight;}coverage/=total;}float alpha=coverage*parameters.tint.a*opacity;output_color=vec4(parameters.tint.rgb*alpha,alpha);return;}
    if(blur>.01){paint=vec4(0);float total=0,sigma=blur/3,step_size=blur/6;for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x){vec2 offset=vec2(x,y)*step_size;float weight=exp(-dot(offset,offset)/(2*sigma*sigma));paint+=input_at(point+offset)*weight;total+=weight;}paint/=total;}else paint=input_at(point);
    paint=over(vec4(parameters.tint.rgb*parameters.tint.a,parameters.tint.a),paint);
    float border_width=parameters.style.z;
    if(border_width>0&&parameters.border.a>0){vec4 inner=parameters.bounds+vec4(border_width,border_width,-2*border_width,-2*border_width);float inside=clamp(.5-distance_to_shape(point,inner,max(0,parameters.style.x-border_width))*scale,0,1);float alpha=clamp((coverage-inside)/max(coverage,.0001),0,1)*parameters.border.a;paint=over(vec4(parameters.border.rgb*alpha,alpha),paint);}
    float light=parameters.highlight.x*clamp((coverage-mask(point-vec2(0,parameters.highlight.y)))/max(coverage,.0001),0,1);paint=over(vec4(light),paint);
    output_color=paint*(coverage*opacity);
}
