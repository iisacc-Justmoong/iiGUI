#include <iiGUI/ModalPresentation.h>
#include <iiGUI/ModalModel.h>
#include <iiGUI/NativeButtonView.h>
#include <iiGUI/AlertButtonModel.h>
#include <stdexcept>
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){
    using namespace ii::gui;
    auto title=ModalPresentation::title("Title",WidgetColor{1,1,1,.9});
    auto detail=ModalPresentation::description("Detail",WidgetColor{1,1,1,.8});
    require(title.text=="Title"&&title.font_size==22&&title.line_height==22&&title.font_weight==700&&
            detail.text=="Detail"&&detail.font_size==13&&detail.line_height==18&&detail.font_weight==500,
            "Modal text style changed");
    ModalModel model;auto first=ModalPresentation::action<ButtonInput,AlertButtonOptions,AlertButtonModel>(0,model.action(0),ButtonTone::primary,ButtonTone::normal);
    auto hidden=ModalPresentation::action<ButtonInput,AlertButtonOptions,AlertButtonModel>(2,model.action(2),ButtonTone::primary,ButtonTone::normal);
    require(first&&first->configuration.text==model.action(0).text&&first->configuration.height==20&&
            first->configuration.tone==ButtonTone::primary&&hidden&&hidden->configuration.tone==ButtonTone::normal,
            "Modal action preparation changed");
    require(!ModalPresentation::action<ButtonInput,AlertButtonOptions,AlertButtonModel>(3,model.action(3),ButtonTone::primary,ButtonTone::normal),
            "Invalid modal action slot accepted");
}
