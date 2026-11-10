#include <iiGUI/TextInputModel.h>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class T>T take(ii::gui::TextEditResult<T> result){if(!result)throw std::runtime_error(result.error().message);return std::move(*result);}}
int main(){using namespace ii::gui;auto text=take(TextInputModel::create(make_text_input_boundary,"A😀Z"));
 check(text.length()==4,"UTF-16 input length changed");take(text.set_selection(1,3));take(text.insert("X"));check(text.text()=="AXZ","Selection replacement failed");
 take(text.undo());check(text.text()=="A😀Z","Undo lost the original text");take(text.redo());check(text.text()=="AXZ","Redo lost the replacement");
 auto before=text.text();check(!text.insert(std::string(TextInputModel::maximum_bytes+1,'x'))&&text.text()==before,"Oversize input changed the model");}
