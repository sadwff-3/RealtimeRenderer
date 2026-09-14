#define STB_IMAGE_IMPLEMENTATION
#include<stb_image.h>

#include"Texture.h"

#include"glad/gl.h"

#include<iostream>

Texture::Texture(const char* path, bool srgb)
{
	glGenTextures(1, &ID);
	glBindTexture(GL_TEXTURE_2D, ID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);

    // filtering
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    
    stbi_set_flip_vertically_on_load(true);

    int width;
    int height;
    int channels;

    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    if (data)
    {
        GLenum dataFormat;
        GLenum internalFormat;

        if (channels == 1)
        {
            dataFormat = GL_RED;
            internalFormat = GL_RED;
        }
        else if (channels == 2)
        {
            dataFormat = GL_RG;
            internalFormat = GL_RG;
        }
        else if (channels == 3)
        {
            dataFormat = GL_RGB;
            internalFormat = srgb ? GL_SRGB : GL_RGB;
        }
        else if (channels == 4)
        {
            dataFormat = GL_RGBA;
            internalFormat = srgb ? GL_SRGB_ALPHA : GL_RGBA;
        }
        else
        {
            std::cerr << "Unsupported texture channels: " << channels << " | " << path << '\n';
            stbi_image_free(data);
            return;
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        std::cout << "Texture loaded: " << path << " (" << width << "x" << height << ", channels=" << channels << ")\n";
    }
}

Texture::Texture(unsigned char r, unsigned char g, unsigned char b)
{
    unsigned char data[] = { r,g,b };
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void Texture::bind() const
{
    glBindTexture(GL_TEXTURE_2D, ID);
}

Texture::~Texture()
{
    glDeleteTextures(1, &ID);
}