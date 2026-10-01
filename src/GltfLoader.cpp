
#include "Model.h"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>

#include <glm/gtc/matrix_inverse.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>
#include <algorithm>

namespace {

    namespace fs = std::filesystem;
    using ByteSpan = std::span<const std::byte>;

    std::vector<std::uint8_t> readFile(const fs::path& path, std::size_t skip = 0) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) return {};

        const auto len = f.tellg();
        if (len < 0 || static_cast<std::uintmax_t>(len) < skip) return {};

        const auto size = static_cast<std::size_t>(len) - skip;
        std::vector<std::uint8_t> bytes(size);

        f.seekg(static_cast<std::streamoff>(skip), std::ios::beg);
        if (!f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return {};

        return bytes;
    }

    ByteSpan bufferBytes(const fastgltf::DataSource& data) {
        if (const auto* arr = std::get_if<fastgltf::sources::Array>(&data)) {
            return { arr->bytes.data(), arr->bytes.size() };
        }

        if (const auto* vec = std::get_if<fastgltf::sources::Vector>(&data)) {
            return { vec->bytes.data(), vec->bytes.size() };
        }

        if (const auto* view = std::get_if<fastgltf::sources::ByteView>(&data)) {
            return { view->bytes.data(), view->bytes.size() };
        }

        return {};
    }

    ByteSpan embeddedImage(const fastgltf::Asset& asset, const fastgltf::sources::BufferView& source) {
        if (source.bufferViewIndex >= asset.bufferViews.size()) return {};

        const auto& view = asset.bufferViews[source.bufferViewIndex];
        if (view.bufferIndex >= asset.buffers.size()) return {};

        const auto bytes = bufferBytes(asset.buffers[view.bufferIndex].data);
        if (view.byteOffset > bytes.size() || view.byteLength > bytes.size() - view.byteOffset) return {};

        return bytes.subspan(view.byteOffset, view.byteLength);
    }

    bool looksLikeKtx2(ByteSpan bytes) {
        constexpr std::uint8_t signature[12] = {
            0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
        };

        if (bytes.size() < 12) return false;

        for (std::size_t i = 0; i < 12; ++i) {
            if (std::to_integer<std::uint8_t>(bytes[i]) != signature[i]) return false;
        }

        return true;
    }

    bool isKtxImage(const fastgltf::Image& image) {
        if (const auto* u = std::get_if<fastgltf::sources::URI>(&image.data)) {
            if (u->mimeType == fastgltf::MimeType::KTX2) return true;

            const auto name = std::string(u->uri.path());
            return name.size() >= 5 && name.substr(name.size() - 5) == ".ktx2";
        }

        if (const auto* b = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
            return b->mimeType == fastgltf::MimeType::KTX2;
        }

        if (const auto* a = std::get_if<fastgltf::sources::Array>(&image.data)) {
            return a->mimeType == fastgltf::MimeType::KTX2;
        }

        if (const auto* b = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
            return b->mimeType == fastgltf::MimeType::KTX2;
        }

        return false;
    }

    glm::mat4 toGlm(const fastgltf::math::fmat4x4& m) {
        glm::mat4 out(1.0f);

        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                out[column][row] = m[column][row];
            }
        }

        return out;
    }

} // namespace

void Model::loadGltf(const std::string& path) {
    const fs::path filePath = fs::absolute(path);
    const fs::path baseDir = filePath.parent_path();

    auto input = fastgltf::GltfDataBuffer::FromPath(filePath);

    if (input.error() != fastgltf::Error::None) {
        std::cerr << "fastgltf: could not open " << path << '\n';
        return;
    }

    fastgltf::Parser parser(
        fastgltf::Extensions::KHR_texture_basisu |
        fastgltf::Extensions::KHR_materials_specular |
        fastgltf::Extensions::KHR_materials_transmission |
        fastgltf::Extensions::KHR_texture_transform
    );

    const auto options = fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadGLBBuffers;
    auto parsed = parser.loadGltf(input.get(), baseDir, options);

    if (parsed.error() != fastgltf::Error::None) {
        std::cerr << "fastgltf: parse error " << static_cast<int>(parsed.error()) << " in " << path << '\n';
        return;
    }

    fastgltf::Asset asset = std::move(parsed.get());
    const bool bistroPacking = filePath.stem().string().starts_with("Bistro");

    // NEW??glTF ?????????????
    auto loadGltfTexture = [&](std::size_t textureIndex, bool srgb) -> Texture* {
        if (textureIndex >= asset.textures.size()) return nullptr;

        const auto& tex = asset.textures[textureIndex];
        const auto selected = tex.basisuImageIndex.has_value() ? tex.basisuImageIndex : tex.imageIndex;

        if (!selected || *selected >= asset.images.size()) return nullptr;

        const auto imageIndex = *selected;
        const auto cacheKey = "gltf:" + std::to_string(imageIndex) + (srgb ? ":srgb" : ":linear");

        if (const auto it = loadedTextures.find(cacheKey); it != loadedTextures.end()) {
            return it->second.get();
        }

        const auto& image = asset.images[imageIndex];
        const bool ktx = isKtxImage(image);
        std::unique_ptr<Texture> texture;

        if (const auto* uri = std::get_if<fastgltf::sources::URI>(&image.data)) {
            if (!uri->uri.isLocalPath()) {
                std::cerr << "glTF remote image URI is unsupported\n";
                return nullptr;
            }

            const auto imagePath = baseDir / uri->uri.fspath();

            if (uri->fileByteOffset == 0) {
                if (ktx) {
                    texture = std::make_unique<Texture>(imagePath.string().c_str(), srgb);
                }
                else {
                    const auto bytes = readFile(imagePath);

                    if (!bytes.empty()) {
                        texture = std::make_unique<Texture>(bytes.data(), bytes.size(), srgb, false);
                    }
                }
            }
            else {
                const auto bytes = readFile(imagePath, uri->fileByteOffset);

                if (!bytes.empty()) {
                    texture = std::make_unique<Texture>(bytes.data(), bytes.size(), srgb, ktx);
                }
            }
        }
        else {
            ByteSpan bytes;

            if (const auto* bv = std::get_if<fastgltf::sources::BufferView>(&image.data)) {
                bytes = embeddedImage(asset, *bv);
            }
            else {
                bytes = bufferBytes(image.data);
            }

            if (!bytes.empty()) {
                texture = std::make_unique<Texture>(
                    reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size(), srgb,
                    ktx || looksLikeKtx2(bytes)
                );
            }
        }

        if (!texture || !texture->valid()) {
            std::cerr << "glTF image " << imageIndex << " failed\n";
            return nullptr;
        }

        auto* result = texture.get();
        loadedTextures.emplace(cacheKey, std::move(texture));
        return result;
        };

    // NEW??glTF ???????
    auto makeMaterial = [&](const auto& index) -> Material {
        Texture* color = defaultDiffuse.get();
        Texture* normal = defaultNormal.get();
        Texture* mr = defaultAO.get();
        Texture* ao = defaultAO.get();

        if (!index || *index >= asset.materials.size()) {
            return Material(color, normal, mr, mr, ao);
        }

        const auto& m = asset.materials[*index];

        if (m.pbrData.baseColorTexture) {
            if (auto* tex = loadGltfTexture(m.pbrData.baseColorTexture->textureIndex, true)) color = tex;
        }

        if (m.normalTexture) {
            if (auto* tex = loadGltfTexture(m.normalTexture->textureIndex, false)) normal = tex;
        }

        if (m.pbrData.metallicRoughnessTexture) {
            if (auto* tex = loadGltfTexture(m.pbrData.metallicRoughnessTexture->textureIndex, false)) mr = tex;
        }

        if (m.occlusionTexture) {
            if (auto* tex = loadGltfTexture(m.occlusionTexture->textureIndex, false)) ao = tex;
        }

        // Bistro ???? ORM ???????????? glTF ????????
        if (bistroPacking && !m.pbrData.metallicRoughnessTexture && m.specular && m.specular->specularTexture) {
            if (auto* packed = loadGltfTexture(m.specular->specularTexture->textureIndex, false)) {
                mr = packed;
                unsigned int maxIndex = 0;
            }
        }

        Material out(color, normal, mr, mr, ao);
        const auto& factor = m.pbrData.baseColorFactor;

        out.baseColorFactor = glm::vec4(factor[0], factor[1], factor[2], factor[3]);
        out.metallicFactor = m.pbrData.metallicFactor;
        out.roughnessFactor = m.pbrData.roughnessFactor;
        out.normalYSign = directXNormals ? -1.0f : 1.0f;
        out.alphaMode = static_cast<int>(m.alphaMode);
        out.alphaCutoff = m.alphaCutoff;

        return out;
        };

    // NEW???????? Mesh ???
    if (asset.scenes.empty()) {
        std::cerr << "fastgltf: model has no scene\n";
        return;
    }

    const auto sceneIndex = asset.defaultScene.has_value() ? *asset.defaultScene : 0;
    if (sceneIndex >= asset.scenes.size()) return;

    std::size_t skipped = 0;

    fastgltf::iterateSceneNodes(
        asset, sceneIndex, fastgltf::math::fmat4x4(),
        [&](fastgltf::Node& node, fastgltf::math::fmat4x4 world) {
            if (!node.meshIndex || *node.meshIndex >= asset.meshes.size()) return;

            const auto transform = toGlm(world);
            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

            for (const auto& primitive : asset.meshes[*node.meshIndex].primitives) {
                if (primitive.type != fastgltf::PrimitiveType::Triangles) {
                    ++skipped;
                    continue;
                }

                const auto* positions = primitive.findAttribute("POSITION");

                if (!positions || positions->accessorIndex >= asset.accessors.size()) {
                    std::cerr << "glTF primitive skipped: missing/invalid POSITION accessor\n";
                    ++skipped;
                    continue;
                }

                const auto& positionAccessor = asset.accessors[positions->accessorIndex];

                if (positionAccessor.type != fastgltf::AccessorType::Vec3) {
                    std::cerr << "glTF primitive skipped: POSITION accessor is not VEC3\n";
                    ++skipped;
                    continue;
                }

                std::vector<Vertex> vertices(positionAccessor.count);

                fastgltf::iterateAccessorWithIndex<glm::vec3>(
                    asset, positionAccessor,
                    [&](const glm::vec3& pos, std::size_t i) {
                        if (i < vertices.size()) {
                            vertices[i].position = glm::vec3(transform * glm::vec4(pos, 1.0f));
                        }
                    }
                );

                for (auto& v : vertices) {
                    v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                    v.texCoord = glm::vec2(0.0f);
                }

                if (const auto* normals = primitive.findAttribute("NORMAL")) {
                    if (normals->accessorIndex < asset.accessors.size()) {
                        const auto& normalAccessor = asset.accessors[normals->accessorIndex];

                        if (normalAccessor.type == fastgltf::AccessorType::Vec3 &&
                            normalAccessor.count == vertices.size()) {
                            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                                asset, normalAccessor,
                                [&](const glm::vec3& normal, std::size_t i) {
                                    if (i < vertices.size()) {
                                        vertices[i].normal = glm::normalize(normalMatrix * normal);
                                    }
                                }
                            );
                        }
                        else {
                            std::cerr << "glTF NORMAL accessor mismatch; using default normals\n";
                        }
                    }
                    else {
                        std::cerr << "glTF NORMAL accessor index out of range; using default normals\n";
                    }
                }

                if (const auto* uv = primitive.findAttribute("TEXCOORD_0")) {
                    if (uv->accessorIndex < asset.accessors.size()) {
                        const auto& uvAccessor = asset.accessors[uv->accessorIndex];

                        if (uvAccessor.type == fastgltf::AccessorType::Vec2 &&
                            uvAccessor.count == vertices.size()) {
                            fastgltf::iterateAccessorWithIndex<glm::vec2>(
                                asset, uvAccessor,
                                [&](const glm::vec2& texCoord, std::size_t i) {
                                    if (i < vertices.size()) {
                                        vertices[i].texCoord = texCoord;
                                    }
                                }
                            );
                        }
                        else {
                            std::cerr << "glTF TEXCOORD_0 accessor mismatch; using default UVs\n";
                        }
                    }
                    else {
                        std::cerr << "glTF TEXCOORD_0 accessor index out of range; using default UVs\n";
                    }
                }

                std::vector<unsigned int> indices;

                if (primitive.indicesAccessor) {
                    if (*primitive.indicesAccessor >= asset.accessors.size()) {
                        std::cerr << "glTF primitive skipped: index accessor out of range\n";
                        ++skipped;
                        continue;
                    }

                    const auto& accessor = asset.accessors[*primitive.indicesAccessor];

                    if (accessor.type != fastgltf::AccessorType::Scalar) {
                        std::cerr << "glTF primitive skipped: index accessor is not SCALAR\n";
                        ++skipped;
                        continue;
                    }

                    indices.resize(accessor.count);

                    fastgltf::iterateAccessorWithIndex<std::uint32_t>(
                        asset, accessor,
                        [&](std::uint32_t idx, std::size_t i) {
                            if (i < indices.size()) {
                                indices[i] = idx;
                            }
                        }
                    );
                }
                else {
                    indices.reserve(vertices.size());

                    for (std::size_t i = 0; i < vertices.size(); ++i) {
                        indices.push_back(static_cast<unsigned int>(i));
                    }
                }

                if (indices.size() < 3) {
                    ++skipped;
                    continue;
                }

                const auto maxIndex = *std::max_element(indices.begin(), indices.end());

                if (maxIndex >= vertices.size()) {
                    std::cerr << "glTF primitive skipped: index "
                        << maxIndex
                        << " exceeds vertex count "
                        << vertices.size()
                        << '\n';
                    ++skipped;
                    continue;
                }

                meshMaterials.push_back(makeMaterial(primitive.materialIndex));

                meshes.emplace_back(vertices, indices);
            }
        }
    );

    loaded = !meshes.empty();

    std::cout << "fastgltf: " << path << ": " << meshes.size()
        << " mesh primitives, " << loadedTextures.size()
        << " distinct GPU textures; skipped " << skipped
        << " unsupported primitives\n";
}

