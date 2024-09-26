#include <vector>
#include <engine.h>
#include <ctti/type_id.hpp>
#include <array>
#include <span>
#include <spanstream>
#include <iostream>
#include <set>

int main() {
    std::set<int> ms {1,2,3,4,5,6,7,8,9,10};
    std::vector<int> mv{ms.lower_bound(3),ms.lower_bound(7)};
    for (auto i : mv) {
        std::cout << i << " ";
    }
    thread_local std::array<char, 2048> buffer{0};
    thread_local std::span bspan(buffer);
    thread_local std::spanstream ss(bspan);
    ss<<std::endl<<"hello world\nline 2";
    std::cout<<buffer.data()<<std::endl;
    buffer[0] = '\0';
    ss.seekp(0);
    std::cout<<buffer.data()<<std::endl;
    ss <<"goodbye world"<<'\0';
    std::cout<<buffer.data()<<std::endl;
}