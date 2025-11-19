#include "requests.hpp"
#include <graphics/framebuffer.hpp>

extern "C" void _kinit(){

    Framebuffer fb;

    while (true) {
        fb.clear(Color{0, 255, 0});
    }
    
}