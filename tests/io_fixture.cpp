#include <chrono>
#include <iostream>
#include <thread>
int main() {
    std::cout<<'R'<<std::flush;
    if (std::cin.get()!='g') return 97;
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    std::cout<<"42"<<std::flush;
}
