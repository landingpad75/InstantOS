#pragma once

#include <cstdint>
#include "color.hpp"

struct Vector2 {
    int x, y;
};

struct Vector3 {
    int x, y, z;
};

struct Rectangle {
    Vector2 pos, dims;
};


class Graphics {
private:
    uint32_t* fb;
    uint64_t width, height, pitch;
public:
    Graphics(uint32_t* fb, uint64_t width, uint64_t height, uint32_t pitch) 
        : fb(fb), width(width), height(height), pitch(pitch) {}

    void fillRect(Rectangle rect, Color color);
    void fillRectBounds(Rectangle rect, Color color, int strokeSize);
    void fillRectRounded(Rectangle rect, Color color, int roundness, int samples);
    void fillRectRoundedBounds(Rectangle rect, Color color, int strokeSize, int roundness, int samples);
};