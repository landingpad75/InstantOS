#include "requests.hpp"
#include <gdt/gdt.hpp>
#include <graphics/framebuffer.hpp>
#include <graphics/console.hpp>

extern "C" void _kinit(){
    GDT gdt;
    
    Framebuffer fb;
    Console console(&fb);
    console.setTextColor({ 255, 0, 0 });

    fb.clear(Color{0, 255, 0});

    console.drawText("YOOO");
}