#include"Mesh.h"
#include <iostream>
#include <stdexcept>

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, bool hasTangents) : vertices(vertices), indices(indices)
{

    if (!hasTangents)
    {
        calculateTangents();
    }

    setupMesh();
}


void Mesh::setupMesh()
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
	
    glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
	glEnableVertexAttribArray(2);

	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
	glEnableVertexAttribArray(3);

	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));
	glEnableVertexAttribArray(4);

	glBindVertexArray(0);
}

void Mesh::draw()const
{
	glBindVertexArray(VAO);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}


void Mesh::calculateTangents()
{
    for (Vertex& vertex : vertices)
    {
        vertex.tangent = glm::vec3(0.0f);
        vertex.bitangent = glm::vec3(0.0f);
    }
    for (std::size_t i = 0; i+2 < indices.size(); i += 3)
    {
        unsigned int i0 = indices[i];
        unsigned int i1 = indices[i + 1];
        unsigned int i2 = indices[i + 2];


        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
            std::cerr << "Invalid mesh indices at triangle " << i / 3
                 << ": (" << i0 << ", " << i1 << ", " << i2 << ")"
                 << ", vertex count = " << vertices.size()
                 << ", index count = " << indices.size() << '\n';
            throw std::out_of_range("Mesh vertex index out of range");
            
        }


        Vertex& v0 = vertices[i0];
        Vertex& v1 = vertices[i1];
        Vertex& v2 = vertices[i2];

        glm::vec3 edge1 = v1.position - v0.position;
        glm::vec3 edge2 = v2.position - v0.position;
        glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
        glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

        float denominator = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;

        if (glm::abs(denominator) < 0.000001f)
        {
            continue;
        }

        float f = 1.0f / denominator;

        glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);

        glm::vec3 bitangent =
            f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);

        tangent = glm::normalize(tangent);
        bitangent = glm::normalize(bitangent);

        v0.tangent += tangent;
        v1.tangent += tangent;
        v2.tangent += tangent;

        v0.bitangent += bitangent;
        v1.bitangent += bitangent;
        v2.bitangent += bitangent;
    }
    
    for (Vertex& vertex : vertices)
    {
        if (glm::length(vertex.tangent) > 0.000001f)
        {
            vertex.tangent = glm::normalize(vertex.tangent);
        }
        if (glm::length(vertex.bitangent) > 0.000001f)
        {
            vertex.bitangent = glm::normalize(vertex.bitangent);
        }
    }
}

Mesh::~Mesh()
{
    if (EBO != 0)
    {
        glDeleteBuffers(1, &EBO);
    }

    if (VBO != 0)
    {
        glDeleteBuffers(1, &VBO);
    }

    if (VAO != 0)
    {
        glDeleteVertexArrays(1, &VAO);
    }
}

Mesh::Mesh(Mesh&& other)noexcept :vertices(std::move(other.vertices)), indices(std::move(other.indices)), VAO(other.VAO), VBO(other.VBO), EBO(other.EBO)
{
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
}

Mesh& Mesh::operator=(Mesh&& other)noexcept
{
    if (this != &other)
    {
        if (EBO != 0)
        {
            glDeleteBuffers(1, &EBO);
        }

        if (VBO != 0)
        {
            glDeleteBuffers(1, &VBO);
        }

        if (VAO != 0)
        {
            glDeleteVertexArrays(1, &VAO);
        }

        vertices = std::move(other.vertices);
        indices = std::move(other.indices);

        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;

        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
    }

    return *this;
}

