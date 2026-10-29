#include "iiGUI/SurfaceRegistry.h"
#include <iostream>
#include <stdexcept>
struct Surface{int& live;explicit Surface(int& value):live(value){++live;}~Surface(){--live;}};
int main(){ii::gui::detail::SurfaceRegistry<Surface> registry;int live=0,created=0;int first=0,second=0;
    auto factory=[&]{++created;return std::make_shared<Surface>(live);};
    auto a=registry.acquire(&first,nullptr,factory),b=registry.acquire(&first,nullptr,factory);if(a!=b||created!=1||live!=1)return 1;
    a.reset();if(live!=1)return 2;b.reset();if(live)return 3;
    auto c=registry.acquire(&first,nullptr,factory),d=registry.acquire(&second,nullptr,factory);if(c==d||live!=2||created!=3)return 4;c.reset();d.reset();
    try{(void)registry.acquire(&first,nullptr,[]()->std::shared_ptr<Surface>{throw std::runtime_error("factory");});return 5;}catch(const std::runtime_error&){}
    auto e=registry.acquire(&first,nullptr,factory);e.reset();if(live||created!=4)return 6;std::cout<<"SURFACE_OWNERSHIP_OK\n";
}
