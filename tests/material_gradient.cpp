#include <iiGUI/MaterialGradient.h>
#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T>T take(ii::core::Result<T> value){if(!value)throw std::runtime_error(value.error().message);return std::move(*value);}
struct Color{double red=.2,green=.4,blue=.8,alpha=1;};
struct Weights{double primary=.4,edge=.1,alpha=.5;};
struct Material {
    Color color;std::uint64_t revision=0;mutable std::size_t weights_calls=0,alpha_calls=0;
    struct Style{double intense_opacity=1,faint_opacity=1;};
    Style resolved()const{return {};};Color displayed_primary()const{return color;}
    double gradient_alpha(double,double,double,double)const{++alpha_calls;return .5;}
    Weights gradient_weights(double,double,double,double)const{++weights_calls;return {};}
};
struct Policy{using Material=::Material;inline static constexpr std::array<double,3> edge_rgb{.1,.5,1};
    static std::uint64_t geometry_revision(const Material& value){return value.revision;}};
}
int main(){
    ii::gui::BasicMaterialGradient<Policy> cache;Material material;
    auto a=take(cache.render(material,20,10,1));require(a&&a->width()==20&&a->height()==10,"Mask size changed");
    auto calls=material.alpha_calls;require(take(cache.render(material,20,10,1))==a&&material.alpha_calls==calls,"Unchanged mask was rerendered");
    auto first=take(cache.render_colored(material,20,10,1));calls=material.weights_calls;
    require(first&&take(cache.render_colored(material,20,10,1))==first&&material.weights_calls==calls,"Colored cache missed");
    material.color.red=.6;auto second=take(cache.render_colored(material,20,10,1));
    require(second!=first&&material.weights_calls==calls,"Accent change recomputed gradient geometry");
    ++material.revision;auto revised=take(cache.render_colored(material,20,10,1));
    require(revised!=second&&material.weights_calls>calls,"Changed gradient source reused stale geometry");
    auto padded=take(cache.render_colored(material,20,10,2,2,1));require(padded&&padded->width()==48&&padded->height()==24,"Padded scale changed");
    require(!cache.render(material,20,10,0)&&!cache.render_colored(material,20,10,1,-1,0),"Invalid raster input accepted");
    material.color.alpha=0;require(!take(cache.render_colored(material,20,10,1)),"Transparent accent produced image");
    require(ii::gui::BasicMaterialGradient<Policy>::blur_padding(100,50,0)==std::pair{0.,0.},"Blur padding changed");
}
