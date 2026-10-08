#pragma once

#include <cstddef>
#include <cstdint>

class Texture
{
public:
    Texture(const char* path, bool srgb = false);
    Texture(unsigned char r, unsigned char g, unsigned char b);

    Texture(const std::uint8_t* bytes, std::size_t size, bool srgb, bool ktx2);

    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator = (const Texture&) = delete;

    void bind() const;
    void setSampler(int minFilter, int magFilter, int wrapS, int wrapT);

    bool valid() const { return ID != 0; }

private:
    unsigned int ID = 0;

    bool loadImage(const std::uint8_t* bytes, std::size_t size, bool srgb, bool flipY);

    bool loadHDR(const std::uint8_t* bytes, std::size_t size, bool flipY);

    bool loadKtx2(const std::uint8_t* bytes, std::size_t size);
};