#include <iiGUI/RadioGroup.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
struct Member final:ii::gui::RadioGroupMember {
    std::string name;bool checked=false,active=false,displayed=false;int updates=0;
    std::function<void()> during;
    std::string_view group()const noexcept override{return name;}
    bool selected()const noexcept override{return checked;}
    bool participating()const noexcept override{return active;}
    void assign(bool value)noexcept override{checked=value;}
    void publish()noexcept override{if(displayed==checked)return;displayed=checked;++updates;if(during){auto callback=during;callback();}}
};
struct NavigationMember final:ii::gui::RadioGroupMember,ii::gui::RadioGroupNavigation {
    std::string name;bool active=true,checked=false;int visits=0;std::shared_ptr<const void> domain=std::make_shared<int>(1);
    std::string_view group()const noexcept override{return name;}
    bool selected()const noexcept override{return checked;}
    bool participating()const noexcept override{return active;}
    void assign(bool value)noexcept override{checked=value;}
    void publish()noexcept override{}
    std::optional<ii::gui::RadioNavigationPosition> navigation_position()const override{return ii::gui::RadioNavigationPosition{1,2,domain};}
    bool navigate_to(std::string_view expected)override{if(!active||name!=expected)return false;++visits;return true;}
};
void grouping(){
    auto groups=std::make_shared<ii::gui::RadioGroups>();auto a=std::make_shared<Member>();auto b=std::make_shared<Member>();auto c=std::make_shared<Member>();
    a->name=b->name=c->name="choice";a->checked=true;
    require(groups->prepare(a)&&groups->prepare(b)&&groups->prepare(c),"Group preparation failed");
    a->active=b->active=true;require(groups->refresh(a)&&groups->refresh(b),"Initial group failed");
    require(a->displayed&&!b->displayed,"Initial selection failed");
    require(groups->select(b,true)&&!a->checked&&b->checked&&!a->displayed&&b->displayed,"Exclusive selection failed");
    c->checked=true;require(groups->select(b,true)&&!c->active&&c->checked&&b->checked,"Unpublished candidate affected live group");
    c->active=true;require(groups->refresh(c)&&!b->checked&&c->checked&&c->displayed,"Committed candidate did not take selection");
    c->during=[&]{c->during={};(void)groups->select(a,true);};
    require(groups->select(b,true)&&a->checked&&!b->checked&&!c->checked&&a->displayed&&!b->displayed&&!c->displayed,
            "Reentrant selection did not settle all group members");
    b->name="other";require(groups->select(b,true)&&a->checked&&b->checked,"Separate groups interfered");
    c->active=false;groups->retire(c);require(groups->select(a,false)&&!a->checked&&b->checked,"Retirement or explicit clear failed");
    auto x=std::make_shared<Member>(),y=std::make_shared<Member>();x->active=y->active=true;x->checked=y->checked=true;
    require(groups->prepare(x)&&groups->prepare(y)&&groups->refresh(x)&&groups->refresh(y)&&x->checked&&y->checked,
            "Unnamed radios were incorrectly coupled");
    auto nav=std::make_shared<NavigationMember>(),foreign=std::make_shared<NavigationMember>(),inactive=std::make_shared<NavigationMember>();
    nav->name=inactive->name="choice";foreign->name="other";inactive->active=false;
    require(groups->prepare(nav)&&groups->prepare(foreign)&&groups->prepare(inactive),"Navigation participants were not registered");
    auto choices=groups->navigation_members("choice");
    require(choices.size()==1&&choices.front()->navigate_to("choice")&&nav->visits==1&&groups->navigation_members("").empty(),
            "Optional navigation ports did not respect group and publication boundaries");
    groups->retire(nav);require(groups->navigation_members("choice").empty(),"A retired navigation member remained available");
}
}
int main(){grouping();}
