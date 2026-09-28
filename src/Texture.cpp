
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include <glad/gl.h>
#include <ktx.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

    // NEW
    std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return {};
        return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
    }

    // NEW
    bool hasExtension(const char* wanted) {
        GLint count = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &count);

        for (GLint i = 0; i < count; ++i) {
            const auto* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
            if (ext && std::string_view(ext) == wanted) return true;
        }

        return false;
    }

    // NEW
    void configureTexture(GLuint id, bool mipmaps) {
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

} // namespace

// CHANGED
Texture::Texture(const char* path, bool srgb) {
    if (!path) return;

    const auto bytes = readBytes(path);

    if (bytes.empty()) {
        std::cerr << "Texture file missing or empty: " << path << '\n';
        return;
    }

    std::string suffix = std::filesystem::path(path).extension().string();
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const bool ok = suffix == ".ktx2"
        ? loadKtx2(bytes.data(), bytes.size())
        : loadImage(bytes.data(), bytes.size(), srgb, true);

    if (!ok) std::cerr << "Texture load failed: " << path << '\n';
}

// NEW
Texture::Texture(const std::uint8_t* bytes, std::size_t size, bool srgb, bool ktx2) {
    if (!bytes || size == 0) return;

    if (ktx2) {
        loadKtx2(bytes, size);
    }
    else {
        loadImage(bytes, size, srgb, false);
    }
}

// NEW
bool Texture::loadImage(const std::uint8_t* bytes, std::size_t size, bool srgb, bool flipY) {
    if (size > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        std::cerr << "PNG/JPEG too large for stb_image\n";
        return false;
    }

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_uc* pixels = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &channels, 0);

    if (!pixels) {
        std::cerr << "stb_image: " << stbi_failure_reason() << '\n';
        return false;
    }

    GLenum format = 0;
    GLenum internal = 0;

    switch (channels) {
    case 1:
        format = GL_RED;
        internal = GL_R8;
        break;
    case 2:
        format = GL_RG;
        internal = GL_RG8;
        break;
    case 3:
        format = GL_RGB;
        internal = srgb ? GL_SRGB8 : GL_RGB8;
        break;
    case 4:
        format = GL_RGBA;
        internal = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        break;
    default:
        stbi_image_free(pixels);
        return false;
    }

    glGenTextures(1, &ID);
    configureTexture(ID, true);

    GLint oldAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlignment);

    stbi_image_free(pixels);

    if (glGetError() != GL_NO_ERROR) {
        glDeleteTextures(1, &ID);
        ID = 0;
        return false;
    }

    return true;
}

// NEW
bool Texture::loadKtx2(const std::uint8_t* bytes, std::size_t size) {
    ktxTexture2* ktx = nullptr;

    const auto result = ktxTexture2_CreateFromMemory(
        bytes, static_cast<ktx_size_t>(size), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktx
    );

    if (result != KTX_SUCCESS || !ktx) {
        std::cerr << "libktx: invalid KTX2 (error " << result << ")\n";
        return false;
    }

    if (ktxTexture2_NeedsTranscoding(ktx)) {
        ktx_transcode_fmt_e target = KTX_TTF_RGBA32;

        if (hasExtension("GL_ARB_texture_compression_bptc")) {
            target = KTX_TTF_BC7_RGBA;
        }
        else if (hasExtension("GL_EXT_texture_compression_s3tc") || hasExtension("GL_S3_s3tc")) {
            target = KTX_TTF_BC3_RGBA;
        }

        const auto transcode = ktxTexture2_TranscodeBasis(ktx, target, 0);

        if (transcode != KTX_SUCCESS) {
            std::cerr << "libktx: Basis transcode failed: " << transcode << '\n';
            ktxTexture_Destroy(ktxTexture(ktx));
            return false;
        }
    }

    GLenum target = 0;
    GLenum glError = GL_NO_ERROR;
    GLuint glTexture = 0;

    const auto upload = ktxTexture_GLUpload(ktxTexture(ktx), &glTexture, &target, &glError);
    const bool mipmaps = ktx->numLevels > 1;
    ktxTexture_Destroy(ktxTexture(ktx));

    if (upload != KTX_SUCCESS || target != GL_TEXTURE_2D) {
        std::cerr << "libktx: GL upload failed (code " << upload << ", GL error " << glError << ")\n";
        if (glTexture) glDeleteTextures(1, &glTexture);
        return false;
    }

    ID = glTexture;
    configureTexture(ID, mipmaps);
    return true;
}

// CHANGED
Texture::Texture(unsigned char r, unsigned char g, unsigned char b) {
    const std::uint8_t pixels[] = { r, g, b };

    glGenTextures(1, &ID);
    configureTexture(ID, false);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

// UNCHANGED
void Texture::bind() const {
    glBindTexture(GL_TEXTURE_2D, ID);
}

// CHANGED
Texture::~Texture() {
    if (ID) glDeleteTextures(1, &ID);
}
