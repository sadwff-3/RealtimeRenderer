#include"EnvironmentMap.h"
#include"Texture.h"

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<array>
#include<iostream>
#include<algorithm>
#include<cctype>
#include<filesystem>


EnvironmentMap::EnvironmentMap() : equirectangularShader("shaders/equirectangular.vert", "shaders/equirectangular.frag"), irradianceShader("shaders/equirectangular.vert", "shaders/irradiance_convolution.frag"), prefilterShader("shaders/equirectangular.vert", "shaders/prefilter.frag"), brdfShader("shaders/brdf.vert", "shaders/brdf.frag")
{
	//environmentMap
	glGenTextures(1, &cubemap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

	for (int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB32F, 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &captureFBO);
	glGenRenderbuffers(1, &captureRBO);
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, cubemap, 0);

	//irradianceMap
	glGenTextures(1, &irradianceMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);

	for (int i = 0; i < 6; ++i)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	//prefilterMap
	glGenTextures(1, &prefilterMap);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

	for (int mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip)
	{
		int mipSize = PREFILTER_SIZE >> mip;

		for (int face = 0; face < 6; ++face)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip, GL_RGB16F, mipSize, mipSize, 0, GL_RGB, GL_FLOAT, nullptr);
		}
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, PREFILTER_MIP_LEVELS - 1);

	//brdfMap
	glGenTextures(1, &brdfLUT);
	glBindTexture(GL_TEXTURE_2D, brdfLUT);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, BRDF_LUT_SIZE, BRDF_LUT_SIZE, 0, GL_RG, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenVertexArrays(1, &brdfVAO);


	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Environment capture framebuffer incomplete\n";
	}
	else
	{
		std::cout << "Environment capture framebuffer complete\n";
	}
		
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	initCube();

	equirectangularShader.use();
	equirectangularShader.setInt("equirectangularMap", 0);
	irradianceShader.use();
	irradianceShader.setInt("environmentMap", 0);
	prefilterShader.use();
	prefilterShader.setInt("environmentMap", 0);

	const GLfloat black[3] = { 0.0f, 0.0f, 0.0f };
	glClearTexImage(cubemap, 0, GL_RGB, GL_FLOAT, black);
	glGenerateTextureMipmap(cubemap);
	glClearTexImage(irradianceMap, 0, GL_RGB, GL_FLOAT, black);
	for (int mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip)
		glClearTexImage(prefilterMap, mip, GL_RGB, GL_FLOAT, black);

	generateBRDFLUT();
}


EnvironmentMap::~EnvironmentMap()
{
	if (cubeVBO != 0)
	{
		glDeleteBuffers(1, &cubeVBO);
	}

	if (cubeVAO != 0)
	{
		glDeleteVertexArrays(1, &cubeVAO);
	}

	if (cubemap != 0)
	{
		glDeleteTextures(1, &cubemap);
	}

	if (captureRBO != 0)
	{
		glDeleteRenderbuffers(1, &captureRBO);
	}

	if (captureFBO != 0)
	{
		glDeleteFramebuffers(1, &captureFBO);
	}

	if (irradianceMap != 0)
	{
		glDeleteTextures(1, &irradianceMap);
	}

	if (prefilterMap != 0)
	{
		glDeleteTextures(1, &prefilterMap);
	}

	if (brdfLUT != 0)
	{
		glDeleteTextures(1, &brdfLUT);
	}

	if (brdfVAO != 0)
	{
		glDeleteVertexArrays(1, &brdfVAO);
	}
}


void EnvironmentMap::initCube()
{
	const float vertices[] = { -1.0f, -1.0f, -1.0f,1.0f,  1.0f, -1.0f,1.0f, -1.0f, -1.0f, 1.0f,  1.0f, -1.0f,-1.0f, -1.0f, -1.0f,	-1.0f,  1.0f, -1.0f,		-1.0f, -1.0f,  1.0f,		 1.0f, -1.0f,  1.0f,		 1.0f,  1.0f,  1.0f,		 1.0f,  1.0f,  1.0f,		-1.0f,  1.0f,  1.0f,		-1.0f, -1.0f,  1.0f,		-1.0f,  1.0f,  1.0f,		-1.0f,  1.0f, -1.0f,		-1.0f, -1.0f, -1.0f,		-1.0f, -1.0f, -1.0f,		-1.0f, -1.0f,  1.0f,		-1.0f,  1.0f,  1.0f, 1.0f,  1.0f,  1.0f, 1.0f, -1.0f, -1.0f, 1.0f,  1.0f, -1.0f,	 1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  1.0f, 1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f,		  1.0f, -1.0f, -1.0f,		  1.0f, -1.0f,  1.0f,	  1.0f, -1.0f,  1.0f,		 -1.0f, -1.0f,  1.0f,	 -1.0f, -1.0f, -1.0f,-1.0f,  1.0f, -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f, 1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f };

	glGenVertexArrays(1, &cubeVAO);

	glGenBuffers(1, &cubeVBO);

	glBindVertexArray(cubeVAO);

	glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);

	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

	glBindVertexArray(0);
}


void EnvironmentMap::renderCube() const
{
	glBindVertexArray(cubeVAO);

	glDrawArrays(GL_TRIANGLES, 0, 36);

	glBindVertexArray(0);
}


bool EnvironmentMap::loadHDR(const std::string& path)
{
	std::string extension = std::filesystem::path(path).extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(),
		[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	if (extension != ".hdr")
		return false;

	Texture hdrTexture(path.c_str());
	if (!hdrTexture.valid())
		return false;

	GLint previousDrawFBO, previousReadFBO, previousProgram;
	GLint previousVAO, previousRenderbuffer, previousDepthFunc;
	GLboolean previousDepthMask;
	const GLboolean previousDepthTest = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean previousBlend = glIsEnabled(GL_BLEND);
	const GLboolean previousScissor = glIsEnabled(GL_SCISSOR_TEST);
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFBO);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFBO);
	glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
	glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
	glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
	glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
	glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);

	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	generate(hdrTexture);
	generateIrradiance();
	generatePrefilter();

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previousDrawFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, previousRenderbuffer);
	glBindVertexArray(previousVAO);
	glUseProgram(previousProgram);
	glDepthFunc(previousDepthFunc);
	glDepthMask(previousDepthMask);
	if (previousDepthTest) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
	if (previousBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
	if (previousScissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
	return true;
}

void EnvironmentMap::generate(Texture& hdrTexture)
{
	if (!hdrTexture.valid())
	{
		std::cerr << "Invalid HDR texture\n";
        return;
	}

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	std::array<glm::mat4, 6> captureViews = {glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f),glm::vec3(0.0f, 0.0f, 1.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);

	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

	equirectangularShader.use();
	equirectangularShader.setMat4("projection", captureProjection);

	glActiveTexture(GL_TEXTURE0);
	
	hdrTexture.bind();

	glViewport(0, 0, 512, 512);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);

	glEnable(GL_DEPTH_TEST);

	if (cullWasEnabled)
	{
		glDisable(GL_CULL_FACE);
	}

	for (int i = 0; i < 6; ++i)
	{
		equirectangularShader.setMat4("view", captureViews[i]);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube();
	}

	if (cullWasEnabled)
	{
		glEnable(GL_CULL_FACE);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Filter the HDR source before irradiance integration.
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	std::cout << "HDR -> Environment Cubemap complete\n";
}


void EnvironmentMap::generateIrradiance()
{
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	std::array<glm::mat4, 6> captureViews = { glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f),glm::vec3(0.0f, 0.0f, 1.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)), glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) };

	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);

	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

	irradianceShader.use();
	irradianceShader.setMat4("projection", captureProjection);

	glActiveTexture(GL_TEXTURE0);

	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	
	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);

	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

	glViewport(0, 0, 32, 32);

	glEnable(GL_DEPTH_TEST);

	if (cullWasEnabled)
	{
		glDisable(GL_CULL_FACE);
	}

	for (int i = 0; i < 6; ++i)
	{
		irradianceShader.setMat4("view", captureViews[i]);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube();
	}

	if (cullWasEnabled)
	{
		glEnable(GL_CULL_FACE);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	std:: cout<< "Environment -> Irradiance Cubemap complete\n";
}


void EnvironmentMap::generatePrefilter()
{
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	std::array<glm::mat4, 6> captureViews = { glm::lookAt(glm::vec3(0.0f), glm::vec3(1.0f,0.0f,0.0f), glm::vec3(0.0f,-1.0f,0.0f)),glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,0.0f,0.0f), glm::vec3(0.0f,-1.0f,0.0f)),glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,1.0f,0.0f), glm::vec3(0.0f,0.0f,1.0f)),glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,-1.0f,0.0f), glm::vec3(0.0f,0.0f,-1.0f)),glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,0.0f,1.0f), glm::vec3(0.0f,-1.0f,0.0f)),glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f,0.0f,-1.0f), glm::vec3(0.0f,-1.0f,0.0f)) };

	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);

	GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

	prefilterShader.use();
	prefilterShader.setMat4("projection", captureProjection);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

	if (cullWasEnabled)
	{
		glDisable(GL_CULL_FACE);
	}


	for (int mip = 0; mip < PREFILTER_MIP_LEVELS; ++mip)
	{
		int mipSize = PREFILTER_SIZE >> mip;

		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipSize, mipSize);

		glViewport(0, 0, mipSize, mipSize);

		float roughness = static_cast<float>(mip) / static_cast<float>(PREFILTER_MIP_LEVELS - 1);

		prefilterShader.setFloat("roughness", roughness);

		for (int face = 0; face < 6; ++face)
		{
			prefilterShader.setMat4("view", captureViews[face]);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, prefilterMap, mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			renderCube();
		}
	}

	if (cullWasEnabled)
	{
		glEnable(GL_CULL_FACE);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	std::cout << "Environment -> Prefilter Cubemap complete\n";
}

void EnvironmentMap::generateBRDFLUT()
{
	GLint oldViewport[4];
	glGetIntegerv(GL_VIEWPORT, oldViewport);

	GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
	glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, BRDF_LUT_SIZE, BRDF_LUT_SIZE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUT, 0);

	glViewport(0, 0, BRDF_LUT_SIZE, BRDF_LUT_SIZE);

	glDisable(GL_DEPTH_TEST);

	brdfShader.use();

	glClear(GL_COLOR_BUFFER_BIT);

	glBindVertexArray(brdfVAO);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);

	if (depthWasEnabled)
	{
		glEnable(GL_DEPTH_TEST);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);

	std::cout << "BRDF LUT complete\n";

}
