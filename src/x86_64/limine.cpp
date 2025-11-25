#include "requests.hpp"
#include <cpu/gdt/gdt.hpp>
#include <cpu/idt/idt.hpp>
#include <cpu/mm/memmgr.hpp>
#include <graphics/framebuffer.hpp>
#include <graphics/console.hpp>

Framebuffer* fb = nullptr;
Console* console = nullptr;

extern "C" void _kinit(){
    GDT gdt;
    IDT idt;

    asm volatile("sti");

    
    MemoryManager mm;
    
    fb = new Framebuffer();
    console = new Console(fb);
    console->setTextColor({ 255, 0, 0 });
    fb->clear(Color{0, 255, 0});

    console->drawText("YOOO");

    delete fb;
    delete console;
}