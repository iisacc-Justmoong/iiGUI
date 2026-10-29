#include "iiGUI/PipeWrite.h"
#include <iostream>
int main(){int fds[2];if(pipe(fds))return 1;close(fds[0]);sigset_t before,after;pthread_sigmask(SIG_SETMASK,nullptr,&before);const char byte='x';const auto result=ii::gui::detail::pipe_write(fds[1],&byte,1);const auto error=errno;close(fds[1]);pthread_sigmask(SIG_SETMASK,nullptr,&after);if(result!=-1||error!=EPIPE||sigismember(&before,SIGPIPE)!=sigismember(&after,SIGPIPE))return 2;std::cout<<"CLOSED_PIPE_SURVIVED\n";}
