#pragma once
#include <cstdint>
#include <tuple>
#include "framebuffer.hpp"

class Console {
private:
    Framebuffer* framebuffer;
    uint64_t posX;
    uint64_t posY;
    Color drawColor;
    Color backgroundColor;
    std::pair<uint64_t, uint64_t> savedCursorPointer { 0, 0 };
    
    enum class AnsiState {
        NORMAL,
        ESCAPE,
        CSI,
        CSI_PARAM
    };
    
    AnsiState ansiState;
    char ansiParams[16][32];
    int ansiParamCount;
    int ansiParamIndex;
    
    const Color ANSI_COLORS[16] = {
        0x000000,
        0xd20f39,
        0x40a02b,
        0xfe640b,
        0x1e66f5,
        0x8839ef,
        0x209fb5,
        0xeff1f5,
        0x4c4f69,
        0xe64553,
        0xa6da95,
        0xdf8e1d,
        0x5555ff,
        0xea76cb,
        0x04a5e5,
        0xdce0e8 
    };
    
    bool bold;
    bool faint;
    bool italic;
    bool underline;
    
    void toString(char* ptr, int64_t num, int radix);
    void toString(char* ptr, uint64_t num, int radix);
    void drawChar(const char c);
    void advance();
    void newLine();
    bool shouldScroll();
    void scroll();
    
    void resetAnsiState();
    void handleAnsiChar(char c);
    void executeAnsiSequence(char finalByte);
    int parseAnsiParam(int index, int defaultValue);
    void handleSGR(int param);
    void handleCursorMovement(char command);
    void handleEraseSequence(char command);

public:
    Console(Framebuffer* framebufferVal);
    void drawText(const char* str);
    void drawNumber(int64_t str);
    void drawHex(uint64_t str);
    void setTextColor(Color color);
    void setBackgroundColor(Color color);
};