#pragma once

class Texture
{
public:
    Texture(const char* path);
    Texture(unsigned char r, unsigned char g, unsigned char b);

    ~Texture();

    void bind() const;

private:
    unsigned int ID;
};