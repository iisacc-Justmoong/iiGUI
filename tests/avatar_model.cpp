#include <iiGUI/AvatarModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void sizes(){
    ii::gui::Avatar avatar;auto options=avatar.configuration();
    require(avatar.image_configuration().width==64&&avatar.image_configuration().height==64,"Large avatar size is incorrect");
    options.size=ii::gui::AvatarSize::small;require(avatar.set_configuration(options),"Small avatar rejected");
    require(avatar.image_configuration().width==24&&avatar.image_configuration().height==24,"Small avatar size is incorrect");
    options.size=ii::gui::AvatarSize::medium;options.width=100;options.height=50;
    require(avatar.set_configuration(options),"Custom avatar extent rejected");auto image_options=avatar.image_configuration();
    require(image_options.width==100&&image_options.height==50&&image_options.circular&&image_options.fit==ii::gui::ImageFit::cover,
            "Avatar did not request a circular cover image");
    ii::gui::Image image;require(image.set_configuration(image_options,ii::gui::ImageSize{200,100}),"Avatar image rejected");
    const auto geometry=image.geometry(100,50);
    require(geometry&&geometry->outer.x==25&&geometry->outer.width==50&&geometry->radius==25&&
        geometry->source.x==.25&&geometry->source.width==.5,"Avatar crop or circular alignment is incorrect");
    options.width=options.height=0;require(avatar.set_configuration(options)&&avatar.image_configuration().width==40,"Medium size fallback failed");
}
void validation(){
    ii::gui::Avatar avatar;const auto original=avatar.configuration();auto invalid=original;invalid.width=-1;
    require(!avatar.set_configuration(invalid)&&avatar.configuration()==original,"Invalid width replaced avatar settings");
    invalid=original;invalid.height=std::numeric_limits<double>::infinity();require(!avatar.set_configuration(invalid),"Infinite height accepted");
    invalid=original;invalid.size=static_cast<ii::gui::AvatarSize>(9);require(!avatar.set_configuration(invalid),"Invalid size accepted");
    invalid=original;invalid.background.red=2;require(!avatar.set_configuration(invalid),"Invalid color accepted");
    invalid=original;invalid.accessibility_name=std::string("x\0y",3);require(!avatar.set_configuration(invalid),"Invalid name accepted");
}
}
int main(){sizes();validation();}
