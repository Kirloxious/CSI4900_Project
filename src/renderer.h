#pragma once

#include <glad/glad.h>

struct Texture
{
    GLuint handle = 0;
    int width = 0;
    int height = 0;

};

struct FrameBuffer
{
    GLuint handle = 0;
    Texture texture;
};

Texture createTexture(int width, int height);
FrameBuffer createFrameBuffer(const Texture texture);   
bool attachTextureToFrameBuffer(const Texture texture, FrameBuffer& frameBuffer);
void blitFrameBuffer(const FrameBuffer frameBuffer);