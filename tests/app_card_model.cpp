#include <iiGUI/AppCardModel.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
void natural_layout(){
    ii::gui::AppCardModel card;
    auto empty=card.natural_size({});require(empty&&near(empty->width,280)&&near(empty->height,37),"Empty card natural size changed");
    auto configuration=card.configuration();configuration.title="Title";require(card.set_configuration(configuration),"Valid title rejected");
    auto title=card.natural_size({.title_width=30});require(title&&near(title->width,280)&&near(title->height,75),"Title-only card size changed");
    configuration.subtitle="Subtitle";require(card.set_configuration(configuration),"Valid subtitle rejected");
    auto both=card.natural_size({.title_width=300,.subtitle_width=80,.content_width=100,.content_height=12});
    require(both&&near(both->width,336)&&near(both->height,102),"Text, padding or body natural size changed");
    auto geometry=card.geometry(both->width,both->height);
    require(geometry&&geometry->header&&geometry->separator&&near(geometry->title.y,18)&&near(geometry->subtitle.y,39)&&
        near(geometry->separator->y,61)&&near(geometry->content.y,72)&&near(geometry->content.height,12),
        "Card sections did not use the supplied visual metrics");
}
void overrides_and_validation(){
    ii::gui::AppCardModel card;auto configuration=card.configuration();configuration.title="A";configuration.width=320;configuration.height=100;
    configuration.shape=ii::gui::CardShape::cylinder;require(card.set_configuration(configuration),"Cylinder configuration rejected");
    auto size=card.natural_size({.content_width=800,.content_height=300});
    require(size&&near(size->width,320)&&near(size->height,100),"Explicit card size did not override natural content");
    auto geometry=card.geometry(320,100);require(geometry&&near(geometry->radius,50),"Cylinder radius did not follow the actual bounds");
    auto tiny=card.geometry(10,10);require(tiny&&tiny->content.width>=0&&tiny->content.height>=0&&near(tiny->radius,5),"Small card created invalid geometry");
    auto bad=configuration;bad.padding=-1;require(!card.set_configuration(bad)&&card.configuration()==configuration,"Invalid padding changed the card");
    bad=configuration;bad.width=std::numeric_limits<double>::infinity();require(!card.set_configuration(bad),"Infinite card size accepted");
    bad=configuration;bad.title=std::string("x\0y",3);require(!card.set_configuration(bad),"Invalid text accepted");
    bad=configuration;bad.shape=static_cast<ii::gui::CardShape>(99);require(!card.set_configuration(bad),"Unknown card shape accepted");
    bad=configuration;bad.background.red=2;require(!card.set_configuration(bad),"Invalid card color accepted");
    require(!card.geometry(-1,10)&&!card.natural_size({.content_height=-1}),"Invalid external layout metrics accepted");
}
void supplied_metrics(){
    ii::gui::AppCardModel card;auto c=card.configuration();c.title="A";c.subtitle="B";
    c.padding=10;c.spacing=5;c.minimum_width=200;c.title_size=20;c.subtitle_size=14;
    c.heading_spacing=3;c.separator_height=2;
    require(card.set_configuration(c),"Application card metrics were rejected");
    const auto size=card.natural_size({.title_width=100,.subtitle_width=30,.content_width=40,.content_height=8});
    require(size&&near(size->width,200)&&near(size->height,77),"Card ignored application layout metrics");
    const auto layout=card.geometry(size->width,size->height);
    require(layout&&layout->separator&&near(layout->title.y,10)&&near(layout->subtitle.y,33)&&
            near(layout->separator->y,52)&&near(layout->separator->height,2)&&
            near(layout->content.y,59)&&near(layout->content.height,8),"Card did not apply supplied section metrics");
    c.title_size=std::numeric_limits<double>::infinity();
    require(!card.set_configuration(c),"Non-finite title size was accepted");
}
}
int main(){natural_layout();overrides_and_validation();supplied_metrics();}
