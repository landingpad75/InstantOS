#include "console.hpp"
#include "font.hpp"
#include <cpu/cereal/cereal.hpp>
#include <memory.h>

Console::Console(Framebuffer* framebufferVal){
    framebuffer = framebufferVal;
    posX = 0;
    posY = 0;
    drawColor = 0xcad3f5;
    backgroundColor = 0x494d64;

    resetAnsiState();
    bold = false;
    faint = false;
    italic = false;
    underline = false;
    
    Cereal::get().initialize();
}

void Console::resetAnsiState() {
    ansiState = AnsiState::NORMAL;
    ansiParamCount = 0;
    ansiParamIndex = 0;
    for (int i = 0; i < 16; i++) {
        ansiParams[i][0] = '\0';
    }
}

int Console::parseAnsiParam(int index, int defaultValue) {
    if (index >= ansiParamCount || ansiParams[index][0] == '\0') {
        return defaultValue;
    }
    
    int result = 0;
    for (int i = 0; ansiParams[index][i] != '\0'; i++) {
        result = result * 10 + (ansiParams[index][i] - '0');
    }
    return result;
}

void Console::handleSGR(int param) {
    switch (param) {
        case 0:
            drawColor = 0xFFFFFF;
            bold = false;
            faint = false;
            italic = false;
            underline = false;
            break;
        case 1:
            bold = true;
            break;
        case 2:
            faint = true;
            break;
        case 3:
            italic = true;
            break;
        case 4:
            underline = true;
            break;
        case 22:
            bold = false;
            faint = false;
            break;
        case 23:
            italic = false;
            break;
        case 24:
            underline = false;
            break;

        case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37:
            drawColor = ANSI_COLORS[param - 30];
            break;

        case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            drawColor = ANSI_COLORS[param - 90 + 8];
            break;

        case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47:
            backgroundColor = ANSI_COLORS[param - 40];
            break;

        case 100: case 101: case 102: case 103:
        case 104: case 105: case 106: case 107:
            backgroundColor = ANSI_COLORS[param - 100 + 8];
            break;
        case 39:
            drawColor = 0xFFFFFF;
            break;
        case 49:
            backgroundColor = 0x00FF00;
            break;
    }
}

void Console::handleCursorMovement(char command) {
    int n, m;
    
    switch (command) {
        case 'A':
            n = parseAnsiParam(0, 1);
            if (posY >= (uint64_t)(n * 16)) {
                posY -= n * 16;
            } else {
                posY = 0;
            }
            break;
        case 'B':
            n = parseAnsiParam(0, 1);
            posY += n * 16;
            if (posY + 16 > framebuffer->getHeight()) {
                posY = framebuffer->getHeight() - 16;
            }
            break;
        case 'C':
            n = parseAnsiParam(0, 1);
            posX += n * 8;
            if (posX >= framebuffer->getWidth()) {
                posX = framebuffer->getWidth() - 8;
            }
            break;
        case 'D':
            n = parseAnsiParam(0, 1);
            if (posX >= (uint64_t)(n * 8)) {
                posX -= n * 8;
            } else {
                posX = 0;
            }
            break;
        case 'H':
        case 'f':
            n = parseAnsiParam(0, 1);
            m = parseAnsiParam(1, 1);
            posY = (n - 1) * 16;
            posX = (m - 1) * 8;
            if (posY + 16 > framebuffer->getHeight()) {
                posY = framebuffer->getHeight() - 16;
            }
            if (posX >= framebuffer->getWidth()) {
                posX = framebuffer->getWidth() - 8;
            }
            break;
        case 'G':
            n = parseAnsiParam(0, 1);
            posX = (n - 1) * 8;
            if (posX >= framebuffer->getWidth()) {
                posX = framebuffer->getWidth() - 8;
            }
            break;
    }
}

void Console::handleEraseSequence(char command) {
    int n = parseAnsiParam(0, 0);
    uint64_t startX, startY, endX, endY;
    
    switch (command) {
        case 'J':
            if (n == 0) {
                startX = posX;
                startY = posY;
                endX = framebuffer->getWidth();
                endY = framebuffer->getHeight();
            } else if (n == 1) {
                startX = 0;
                startY = 0;
                endX = posX;
                endY = posY;
            } else if (n == 2 || n == 3) {
                startX = 0;
                startY = 0;
                endX = framebuffer->getWidth();
                endY = framebuffer->getHeight();
                posX = 0;
                posY = 0;
            } else {
                return;
            }
            
            for (uint64_t y = startY; y < endY; y++) {
                for (uint64_t x = (y == startY ? startX : 0); 
                     x < (y == endY - 1 ? endX : framebuffer->getWidth()); x++) {
                    framebuffer->putPixel(x, y, backgroundColor);
                }
            }
            break;
            
        case 'K':
            if (n == 0) {
                for (uint64_t x = posX; x < framebuffer->getWidth(); x++) {
                    for (uint64_t y = posY; y < posY + 16; y++) {
                        framebuffer->putPixel(x, y, backgroundColor);
                    }
                }
            } else if (n == 1) {
                for (uint64_t x = 0; x <= posX; x++) {
                    for (uint64_t y = posY; y < posY + 16; y++) {
                        framebuffer->putPixel(x, y, backgroundColor);
                    }
                }
            } else if (n == 2) {
                for (uint64_t x = 0; x < framebuffer->getWidth(); x++) {
                    for (uint64_t y = posY; y < posY + 16; y++) {
                        framebuffer->putPixel(x, y, backgroundColor);
                    }
                }
            }
            break;
    }
}

void Console::executeAnsiSequence(char finalByte) {
    switch (finalByte) {
        case 'm':
            if (ansiParamCount == 0) {
                handleSGR(0);
            } else {
                for (int i = 0; i < ansiParamCount; i++) {
                    handleSGR(parseAnsiParam(i, 0));
                }
            }
            break;
        case 'A': case 'B': case 'C': case 'D':
        case 'H': case 'f': case 'G':
            handleCursorMovement(finalByte);
            break;
        case 'J': case 'K':
            handleEraseSequence(finalByte);
            break;
        case 's':
            savedCursorPointer = { posX, posY };
            break;
        case 'u':
            posX = savedCursorPointer.first;
            posY = savedCursorPointer.second;
            break;
    }
    
    resetAnsiState();
}

void Console::handleAnsiChar(char c) {
    switch (ansiState) {
        case AnsiState::NORMAL:
            if (c == '\x1b') {
                ansiState = AnsiState::ESCAPE;
            } else {
                if (c == '\n') {
                    newLine();
                } else if (c == '\t') {
                    drawText("    ");
                } else if (c == '\r') {
                    posX = 0;
                } else if (c == '\b') {
                    posX -= 8;
                    drawChar(' ');
                } else {
                    drawChar(c);
                    advance();
                }
            }
            break;
            
        case AnsiState::ESCAPE:
            if (c == '[') {
                ansiState = AnsiState::CSI;
                ansiParamCount = 1;
                ansiParamIndex = 0;
            } else {
                resetAnsiState();
            }
            break;
            
        case AnsiState::CSI:
            if (c >= '0' && c <= '9') {
                int len = 0;
                while (ansiParams[ansiParamCount - 1][len] != '\0') len++;
                if (len < 31) {
                    ansiParams[ansiParamCount - 1][len] = c;
                    ansiParams[ansiParamCount - 1][len + 1] = '\0';
                }
            } else if (c == ';') {
                if (ansiParamCount < 16) {
                    ansiParamCount++;
                    ansiParams[ansiParamCount - 1][0] = '\0';
                }
            } else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                executeAnsiSequence(c);
            } else {
                resetAnsiState();
            }
            break;
            
        default:
            resetAnsiState();
            break;
    }
}

void Console::toString(char* ptr, int64_t num, int radix){
    char* ptr1 = ptr;
    char tmp_char;
    int tmp_value;
    bool negative = false;

    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    if (num < 0 && radix == 10) {
        negative = true;
        num = -num;
    }

    while (num != 0) {
        tmp_value = num % radix;
        num /= radix;
        
        if (tmp_value < 10)
            *ptr++ = "0123456789"[tmp_value];
        else
            *ptr++ = "abcdefghijklmnopqrstuvwxyz"[tmp_value - 10];
    }

    if (negative)
        *ptr++ = '-';

    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void Console::toString(char* ptr, uint64_t num, int radix){
    char* ptr1 = ptr;
    char tmp_char;
    int tmp_value;

    if (num == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    while (num != 0) {
        tmp_value = num % radix;
        num /= radix;
        
        if (tmp_value < 10)
            *ptr++ = "0123456789"[tmp_value];
        else
            *ptr++ = "abcdefghijklmnopqrstuvwxyz"[tmp_value - 10];
    }

    *ptr = '\0';

    ptr--;
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void Console::drawChar(const char c){
    if ((unsigned char)c < 0x20 || (unsigned char)c > 0x7F) return;

    const uint8_t* glyph = font_8x16[c - 0x20];

    uint32_t baseX = posX;
    uint32_t baseY = posY;

    for (uint64_t y = 0; y < 16; y++) {
        uint8_t data = glyph[y];
        for (uint64_t x = 0; x < 8; x++) {
            if (data & (0x80 >> x)) {
                uint64_t locX = baseX + x;
                uint64_t locY = baseY + y;
            
                if (locX < framebuffer->getWidth() && locY < framebuffer->getHeight()) {
                    framebuffer->putPixel(locX, locY, drawColor);
                }
            } else {
                uint64_t locX = baseX + x;
                uint64_t locY = baseY + y;
                
                if (locX < framebuffer->getWidth() && locY < framebuffer->getHeight()) {
                    framebuffer->putPixel(locX, locY, backgroundColor);
                }
            }
        }
    }
}

void Console::drawNumber(int64_t str){
    char hi[512];
    toString(hi, str, 10);
    drawText(hi);
}

void Console::drawHex(uint64_t str){
    char hi[512];
    toString(hi, str, 16);
    drawText(hi);
}

void Console::advance() {
    posX += 8;
    if (posX >= framebuffer->getWidth()) {
        newLine();
    }
}

void Console::newLine() {
    posX = 0;
    posY += 16;
    
    if(shouldScroll()){
        scroll();
    }
}

bool Console::shouldScroll(){
    return posY + 16 > framebuffer->getHeight();
}

void Console::scroll(){
    uint64_t scrollHeight = 16;
    uint64_t LineBytes = framebuffer->getWidth() * sizeof(uint32_t);
    uint8_t* fb = (uint8_t*)framebuffer->getRaw();
    Framebuffer* temp = framebuffer;

    auto getMemAddress = [temp, fb](uint32_t x, uint32_t y) -> uint8_t* {
        return fb + y * temp->getPitch() + x * 4;
    };

    for (uint64_t y = scrollHeight; y < framebuffer->getHeight(); y++) {
        auto dest = getMemAddress(0, y - scrollHeight);
        auto src = getMemAddress(0, y);
        memcpy(dest, src, LineBytes);
    }
    
    for (uint64_t y = framebuffer->getHeight() - scrollHeight; y < framebuffer->getHeight(); y++) {
        for (uint64_t x = 0; x < framebuffer->getWidth(); x++) {
            framebuffer->putPixel(x, y, backgroundColor);
        }
    }
    
    posY -= scrollHeight;
}

void Console::drawText(const char* str){
    Cereal::get().write(str);
    while (*str) {
        handleAnsiChar(*str);
        str++;
    }
}

void Console::setTextColor(Color color){
    drawColor = color;
}

void Console::setBackgroundColor(Color color){
    backgroundColor = color;
}