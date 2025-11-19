[bits 64]

global _main
extern initConstructors
extern _kinit

__cxa_pure_virtual:
    hlt
    jmp __cxa_pure_virtual

_main:
    call initConstructors ; initialize C+++ constructors
    
    call _kinit

.halt:
    hlt
    jmp .halt