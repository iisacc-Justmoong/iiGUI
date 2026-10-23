#version 450
layout(set=0,binding=1) uniform sampler2D source_image;
layout(push_constant) uniform Parameters {vec4 view;vec4 bounds;vec4 tint;vec4 border;vec4 style;} parameters;
layout(location=0) out vec4 output_color;
float distance_to_shape(vec2 point,vec4 bounds,float radius){
    vec2 q=abs(point-bounds.xy-bounds.zw*.5)-(bounds.zw*.5-radius);
    return length(max(q,0))+min(max(q.x,q.y),0)-radius;
}
float mask(vec2 point){return clamp(.5-distance_to_shape(point,parameters.bounds,parameters.style.x)*parameters.view.z,0,1);}
void main(){
    float scale=parameters.view.z,opacity=parameters.view.w,blur=parameters.style.y;
    vec2 point=gl_FragCoord.xy/scale;
    float distance=distance_to_shape(point,parameters.bounds,parameters.style.x);
    bool shadow=parameters.style.w>.5;
    if(opacity<=0||distance>(shadow?blur+1:1)){output_color=vec4(0);return;}
    float coverage=mask(point);
    vec3 paint;
    if(shadow){
        if(blur>.01){coverage=0;float total=0,sigma=blur/3,step_size=blur/6;
            for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x){vec2 offset=vec2(x,y)*step_size;float weight=exp(-dot(offset,offset)/(2*sigma*sigma));coverage+=mask(point+offset)*weight;total+=weight;}coverage/=total;}
        float alpha=coverage*parameters.tint.a*opacity;output_color=vec4(parameters.tint.rgb*alpha,alpha);return;
    }
    vec2 extent=parameters.view.xy*scale;
    if(blur>.01){paint=vec3(0);float total=0,sigma=blur/3,step_size=blur/6;
        for(int y=-6;y<=6;++y)for(int x=-6;x<=6;++x){vec2 offset=vec2(x,y)*step_size;float weight=exp(-dot(offset,offset)/(2*sigma*sigma));paint+=texture(source_image,(gl_FragCoord.xy+offset*scale)/extent).rgb*weight;total+=weight;}paint/=total;
    }else paint=texture(source_image,gl_FragCoord.xy/extent).rgb;
    paint=mix(paint,parameters.tint.rgb,parameters.tint.a);
    float border_width=parameters.style.z;
    if(border_width>0&&parameters.border.a>0){vec4 inner=parameters.bounds+vec4(border_width,border_width,-2*border_width,-2*border_width);float inner_coverage=clamp(.5-distance_to_shape(point,inner,max(0,parameters.style.x-border_width))*scale,0,1);float amount=clamp((coverage-inner_coverage)/max(coverage,.0001),0,1);paint=mix(paint,parameters.border.rgb,amount*parameters.border.a);}
    float alpha=coverage*opacity;output_color=vec4(paint*alpha,alpha);
}
