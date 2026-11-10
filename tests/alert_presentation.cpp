#include <iiGUI/AlertPresentation.h>
#include <iiGUI/AlertModel.h>
#include <iiGUI/AlertButtonModel.h>
#include <cmath>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    using namespace ii::gui;
    auto icon=AlertPresentation::icon({},86,3,WidgetColor{.2,.3,.4,.5},WidgetColor{.6,.7,.8,.9});
    require(icon.configuration.width==86&&icon.configuration.height==86&&icon.configuration.padding==15&&
            icon.configuration.corner_radius==28&&icon.configuration.border_width==1&&
            icon.configuration.background==WidgetColor{.2,.3,.4,.5}&&icon.configuration.border==WidgetColor{.6,.7,.8,.9}&&
            icon.configuration.fit==ImageFit::contain&&icon.configuration.allow_empty,"Alert icon geometry changed");
    auto compact=AlertPresentation::icon({},86,2,WidgetColor{},WidgetColor{});
    require(compact.configuration.padding==11,"Alert compact icon padding changed");
    auto title=AlertPresentation::title("Title",WidgetColor{.2,.3,.4,1});
    auto description=AlertPresentation::description("Detail",WidgetColor{.4,.3,.2,1});
    require(title.text=="Title"&&title.font_size==26&&title.line_height==26&&title.font_weight==700&&title.alignment==TextAlignment::center&&
            description.text=="Detail"&&description.font_size==13&&description.font_weight==500&&description.alignment==TextAlignment::center,
            "Alert typography changed");
    AlertModel model;auto action=AlertPresentation::action<AlertButtonOptions,AlertButtonModel>(model,0);
    require(action&&action->text==model.action(0).text&&action->dialog_style&&action->height==model.configuration().metrics.action_height,
            "Alert action composition changed");
    require(bool(AlertPresentation::action<AlertButtonOptions,AlertButtonModel>(model,1)),
            "A reserved hidden alert slot could not be prepared");
    require(!AlertPresentation::action<AlertButtonOptions,AlertButtonModel>(model,3),
            "An out-of-range alert action was accepted");
}
