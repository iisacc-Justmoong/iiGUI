#pragma once
#include "Presentation.h"
namespace ii::gui::detail {
template<class Draw,class Info,class Now>
void draw_with_presentation(const std::shared_ptr<PresentationStream>& source,Draw draw,Info info,Now now){
    if(!source||!source->has_observers()){draw();return;}
    const auto before=info();
    draw();
    const auto after=info();
    const bool replaced=before.api!=after.api||before.fallback_reason!=after.fallback_reason;
    if(after.presentations>before.presentations||(replaced&&after.presentations))
        (void)source->queued(now(),after.api);
}
}
