#include <iiGUI/CardModel.h>
#include <stdexcept>
namespace {void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}}
int main(){using namespace ii::gui;CardModel card;auto c=card.configuration();check(card.intrinsic_size().width==210,"Default card width changed");
 c.type=CardType::project;c.progress=.5;c.title="Project";check(card.set_configuration(c)&&card.can_select()&&card.shows_progress(),"Project card failed");
 auto g=card.geometry(256,280);check(g&&g->progress_track.width>0&&g->progress_fill.width*2==g->progress_track.width,"Progress geometry changed");
 auto bad=c;bad.progress=2;check(!card.set_configuration(bad)&&card.configuration()==c,"Invalid card replaced state");}
