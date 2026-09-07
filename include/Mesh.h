#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <vector>

struct Vertex
{
    glm::vec3 position{ 0.0f };
    glm::vec3 normal{ 0.0f };
    glm::vec2 texCoord{ 0.0f };

    glm::vec3 tangent{ 0.0f };
    glm::vec3 bitangent{ 0.0f };
};

class Mesh
{
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    void draw() const;

private:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
 
    unsigned int VAO;
    unsigned int VBO;
    unsigned int EBO;

    void calculateTangents();
    void setupMesh();
};
   