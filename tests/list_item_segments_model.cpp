#include <iiGUI/ListItemSegmentsModel.h>
#include <limits>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){using namespace ii::gui;ListItemSegmentsModel model;auto c=model.configuration();
 check(model.intrinsic_size().width==114&&model.intrinsic_size().height==22,"Default segment geometry is not tight-fitting");
 c.options={{"A",true,{}},{"B",false,{}},{"C",true,{}}};check(model.set_configuration(c),"Valid options rejected");
 check(!model.choose(1)&&model.navigate(1)==2&&!model.choose(2),"Disabled or unchanged selection accepted");
 auto bad=model.configuration();bad.selected_index=3;check(!model.set_configuration(bad)&&model.configuration().selected_index==2,"Invalid selection replaced state");
 bad=model.configuration();bad.segment_width=std::numeric_limits<double>::infinity();check(!model.set_configuration(bad),"Infinite extent accepted");
 bad=model.configuration();bad.options[0].text=std::string("bad\xff",4);check(!model.set_configuration(bad),"Invalid UTF-8 accepted");
 c=model.configuration();c.options.clear();c.selected_index=0;check(model.set_configuration(c)&&model.intrinsic_size().height==0&&!model.choose(0),"Empty options failed");}
