
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

    void configureAnisotropy(GLint minFilter, GLint magFilter) {
        GLint major = 0;
        GLint minor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        if (!(major > 4 || (major == 4 && minor >= 6)) && !hasExtension("GL_EXT_texture_filter_anisotropic") && !hasExtension("GL_ARB_texture_filter_anisotropic")) return;

        // The EXT, ARB and OpenGL 4.6 tokens have identical values.
        constexpr GLenum textureMaxAnisotropy = 0x84FE;
        constexpr GLenum maxTextureMaxAnisotropy = 0x84FF;
        GLfloat maximum = 1.0f;
        glGetFloatv(maxTextureMaxAnisotropy, &maximum);
        const bool linear = magFilter == GL_LINEAR && (minFilter == GL_LINEAR || minFilter == GL_LINEAR_MIPMAP_LINEAR || minFilter == GL_LINEAR_MIPMAP_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, textureMaxAnisotropy, linear ? std::clamp(maximum, 1.0f, 16.0f) : 1.0f);
    }

    // NEW
    void configureTexture(GLuint id, bool mipmaps) {
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        configureAnisotropy(mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR, GL_LINEAR);
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

    bool ok = false;

    if (suffix == ".ktx2")
    {
        ok = loadKtx2(bytes.data(), bytes.size());
    }
    else if (suffix == ".hdr")
    {
        ok = loadHDR(bytes.data(), bytes.size(), true);
    }
    else
    {
        ok = loadImage(bytes.data(), bytes.size(), srgb, true);
    }

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

bool Texture::loadHDR(const std::uint8_t* bytes, std::size_t size, bool flipY)
{
    if (size == 0)
    {
        std::cerr << "HDR file empty\n";
        return false;
    }

    stbi_set_flip_vertically_on_load(flipY ? 1 : 0);

    int width = 0;
    int height = 0;
    int channels = 0;

    float* pixels = stbi_loadf_from_memory(bytes, static_cast<int>(size), &width, &height, &channels, 0);

    if (!pixels)
    {
        std::cerr << "stb_image HDR: " << stbi_failure_reason() << '\n';
        return false;
    }

    GLenum format = 0;
    GLenum internal = 0;

    switch (channels)
    {
    case 1:
        format = GL_RED;
        internal = GL_R16F;
        break;

    case 2:
        format = GL_RG;
        internal = GL_RG16F;
        break;

    case 3:
        format = GL_RGB;
        internal = GL_RGB32F;
        break;

    case 4:
        format = GL_RGBA;
        internal = GL_RGBA16F;
        break;

    default:
        stbi_image_free(pixels);
        return false;
    }

    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, format, GL_FLOAT, pixels);

    stbi_image_free(pixels);

    if (glGetError() != GL_NO_ERROR)
    {
        glDeleteTextures(1, &ID);
        ID = 0;
        return false;
    }

    std::cout << "HDR loaded: " << width << "x" << height << ", channels=" << channels << '\n';

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

    // 必须在销毁 ktx 前保存这些信息。
    const unsigned int sourceLevels = ktx->numLevels;
    const unsigned int width = ktx->baseWidth;
    const unsigned int height = ktx->baseHeight;

    const auto upload = ktxTexture_GLUpload(
        ktxTexture(ktx), &glTexture, &target, &glError
    );

    ktxTexture_Destroy(ktxTexture(ktx));

    if (upload != KTX_SUCCESS ||
        target != GL_TEXTURE_2D ||
        glError != GL_NO_ERROR)
    {
        std::cerr << "libktx: GL upload failed (code "
            << upload << ", GL error " << glError << ")\n";

        if (glTexture)
            glDeleteTextures(1, &glTexture);

        return false;
    }

    glBindTexture(GL_TEXTURE_2D, glTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);

    unsigned int finalLevels = sourceLevels;

    // 已经带 mip 的资产保留原来的 mip。
    // 单级纹理则生成完整 mip 链。
    if (sourceLevels == 1 && (width > 1 || height > 1))
    {
        unsigned int largestSize = std::max(width, height);
        finalLevels = 1;

        while (largestSize > 1)
        {
            largestSize >>= 1;
            ++finalLevels;
        }

        // libktx 上传单级纹理时会把 MAX_LEVEL 设置成 0。
        // 必须先解除这个限制，再生成 mip。
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAX_LEVEL,
            static_cast<GLint>(finalLevels - 1)
        );

        glGenerateMipmap(GL_TEXTURE_2D);

        const GLenum mipError = glGetError();

        if (mipError != GL_NO_ERROR)
        {
            std::cerr << "KTX2 mip generation failed: "
                << mipError << '\n';

            glDeleteTextures(1, &glTexture);
            return false;
        }
    }

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAX_LEVEL,
        static_cast<GLint>(finalLevels - 1)
    );

    ID = glTexture;
    configureTexture(ID, finalLevels > 1);

    return true;
}

// CHANGED
Texture::Texture(unsigned char r, unsigned char g, unsigned char b) {
    const std::uint8_t pixels[] = { r, g, b };

    glGenTextures(1, &ID);
    configureTexture(ID, false);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
}

void Texture::setSampler(int minFilter, int magFilter, int wrapS, int wrapT) {
    if (!ID) return;
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    configureAnisotropy(minFilter, magFilter);
}

// UNCHANGED
void Texture::bind() const {
    glBindTexture(GL_TEXTURE_2D, ID);
}

// CHANGED
Texture::~Texture() {
    if (ID) glDeleteTextures(1, &ID);
}
