#include "../src/uri.h"
#include <iostream>

int main(int argc, char** argv) {
    //HPS::Uri::ptr uri = HPS::Uri::Create("http://www.HPS.top/test/uri?id=100&name=HPS#frg");
    //HPS::Uri::ptr uri = HPS::Uri::Create("http://admin@www.HPS.top/test/中文/uri?id=100&name=HPS&vv=中文#frg中文");
    HPS::Uri::ptr uri = HPS::Uri::Create("http://admin@www.HPS.top");
    //HPS::Uri::ptr uri = HPS::Uri::Create("http://www.HPS.top/test/uri");
    std::cout << uri->toString() << std::endl;
    auto addr = uri->createAddress();
    std::cout << *addr << std::endl;
    return 0;
}
