#include"Shader.h"
#include<glad/gl.h>
#include<glm/gtc/type_ptr.hpp>
#include<fstream>
#include<sstream>
#include<iostream>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
	std::ifstream vertexFile(vertexPath);
	std::ifstream fragmentFile(fragmentPath);

	if (!vertexFile.is_open())
	{
		std::cerr
			<< "Failed to open vertex shader: "
			<< vertexPath
			<< '\n';
	}
	if (!fragmentFile.is_open())
	{
		std::cerr
			<< "Failed to open fragment shader: "
			<< fragmentPath
			<< '\n';
	}
	std::stringstream vertexStream;
	std::stringstream fragmentStream;

	vertexStream << vertexFile.rdbuf();
	fragmentStream << fragmentFile.rdbuf();

	std::string vertexCode = vertexStream.str();
	std::string fragmentCode = fragmentStream.str();

	const char* vertexSource = vertexCode.c_str();
	const char* fragmentSource = fragmentCode.c_str();

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);

	glShaderSource(vertexShader, 1, &vertexSource, nullptr);
	glCompileShader(vertexShader);
	checkCompileErrors(vertexShader, "VERTEX");

	unsigned int fragmentShader =glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(fragmentShader,1,&fragmentSource,nullptr);
	glCompileShader(fragmentShader);
	checkCompileErrors(fragmentShader,"FRAGMENT");

	ID = glCreateProgram();
	glAttachShader(ID, vertexShader);
	glAttachShader(ID, fragmentShader);
	glLinkProgram(ID);
	checkCompileErrors(ID, "PROGRAM");
	
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void Shader::use() const
{
	glUseProgram(ID);
}

void Shader::checkCompileErrors(unsigned int object,const std::string& type)
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM")
    {
        glGetShaderiv(object,GL_COMPILE_STATUS,&success);

        if (!success)
        {
            glGetShaderInfoLog(object,1024,nullptr,infoLog);

            std::cerr
                << "Shader compilation error ["
                << type
                << "]:\n"
                << infoLog
                << '\n';
        }
    }
    else
    {
        glGetProgramiv(object,GL_LINK_STATUS,&success
        );

        if (!success)
        {
            glGetProgramInfoLog(object,1024,nullptr,infoLog);

            std::cerr
                << "Shader program linking error:\n"
                << infoLog
                << '\n';
        }
    }
}

void Shader::setInt(const std::string& name,int value) const
{
	glUniform1i(glGetUniformLocation(ID, name.c_str()),value);
}

void Shader::setMat4(const std::string& name, const glm::mat4& matrix) const
{
	glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::setVec3(const std::string& name, const glm::vec3& value)const
{
	glUniform3fv(glGetUniformLocation(ID, name.c_str()),1,glm::value_ptr(value));
}

void Shader::setFloat(const std::string& name, float value)const
{
	glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}