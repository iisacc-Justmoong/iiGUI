#include <iiGUI/AlertModel.h>
#include <limits>
#include <stdexcept>
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
    ii::gui::AlertModel model;auto config=model.configuration();
    config.title="Title";config.message="Description";config.action_text={"Apply","Cancel",""};
    require(model.set_configuration(config)&&model.action_count()==2&&model.description()=="Description", "Alert input was rejected");
    auto layout=model.layout(800,600,26,26);
    require(layout&&layout->frame.width==500&&layout->frame.height==379&&layout->actions[0].x==257&&layout->actions[1].width==219,
            "Alert default geometry changed");
    config.metrics.action_horizontal_inset=40;config.metrics.action_gap=10;
    require(model.set_configuration(config),"Alert rejected caller-supplied metrics");
    layout=model.layout(800,600,26,26);
    require(layout&&layout->actions[1].x==40&&layout->actions[1].width==205&&layout->actions[0].x==255,
            "Alert ignored caller-supplied metrics");
    auto previous=model.configuration();config.metrics.action_gap=-1;
    require(!model.set_configuration(config)&&model.configuration()==previous,"Invalid metrics replaced a valid alert");
    config=previous;config.action_count=3;config.secondary_destructive=false;
    require(model.set_configuration(config)&&!model.action(1).destructive&&!model.action(3).visible,
            "Alert action policy was not preserved");
    require(!model.layout(-1,400,20,20)&&!model.layout(400,400,std::numeric_limits<double>::infinity(),20),
            "Invalid alert measurements were accepted");
}
