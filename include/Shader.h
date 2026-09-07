#pragma once

#include <string>
#include <glm/glm.hpp>
class Shader
{
public:
    unsigned int ID;

    Shader(
        const char* vertexPath,
        const char* fragmentPath
    );

    void use() const;
    void setInt(
    const std::string& name,
    int value
) const;
    
    void setVec3(
    const std::string& name,
    const glm::vec3& value
) const;

    void setMat4(
    const std::string& name,
    const glm::mat4& matrix
) const;

    void setFloat(
    const std::string& name,
    float value
) const;

private:
    void checkCompileErrors(
        unsigned int object,
        const std::string& type
    );
};