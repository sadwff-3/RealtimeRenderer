#include"Renderer.h"
#include"Scene.h"
#include"SceneObject.h"
#include"Model.h"
#include "Camera.h"
#include"Mesh.h"

#include <string>
#include<glad/gl.h>
#include<glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <algorithm>

Renderer::Renderer(int width, int height) :shadowShader("shaders/shadow_depth.vert", "shaders/shadow_depth.frag"), sceneShader("shaders/pbr.vert", "shaders/pbr.frag"), screenShader("shaders/screen.vert", "shaders/screen.frag"), geometryShader("shaders/gbuffer.vert", "shaders/gbuffer.frag"), deferredShader("shaders/screen.vert", "shaders/deferred_lighting.frag"), framebufferWidth(width), framebufferHeight(height)
{
	initShadowMap();
	initSceneFramebuffer();
	initGBuffer();
	deferredShader.use();
	deferredShader.setInt("gNormal", 0);
	deferredShader.setInt("gAlbedo", 1);
	deferredShader.setInt("gMaterial", 2);
	deferredShader.setInt("gDepth", 3);
	deferredShader.setInt("shadowMap", 4);
	sceneShader.use();
	sceneShader.setInt("shadowMap", 2);
	screenShader.use();
	screenShader.setInt("screenTexture", 0);
	screenShader.setFloat("exposure", 1.0f);
}

Renderer::~Renderer()
{
	if (depthMap != 0)
	{
		glDeleteTextures(1, &depthMap);
	}

	if (depthMapFBO != 0)
	{
		glDeleteFramebuffers(1, &depthMapFBO);
	}

	if (sceneColorTexture != 0)
	{
		glDeleteTextures(1, &sceneColorTexture);
	}

	if (sceneRBO != 0)
	{
		glDeleteRenderbuffers(1, &sceneRBO);
	}

	if (sceneFBO != 0)
	{
		glDeleteFramebuffers(1, &sceneFBO);
	}

	if (gNormal != 0)
	{
		glDeleteTextures(1, &gNormal);
	}

	if (gAlbedo != 0)
	{
		glDeleteTextures(1, &gAlbedo);
	}

	if (gMaterial != 0)
	{
		glDeleteTextures(1, &gMaterial);
	}

	if (gDepth != 0)
	{
		glDeleteTextures(1, &gDepth);
	}

	if (gBuffer != 0)
	{
		glDeleteFramebuffers(1, &gBuffer);
	}
}

void Renderer::initShadowMap()
{
	glGenFramebuffers(1, &depthMapFBO);

	glGenTextures(1, &depthMap);
	glBindTexture(GL_TEXTURE_2D, depthMap);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::initSceneFramebuffer()
{
	glGenFramebuffers(1, &sceneFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);

	glGenTextures(1, &sceneColorTexture);
	glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColorTexture, 0);

	glGenRenderbuffers(1, &sceneRBO);
	glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebufferWidth, framebufferHeight);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, sceneRBO);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Scene framebuffer is not complete!\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::initGBuffer()
{
	glGenFramebuffers(1, &gBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

	glGenTextures(1, &gNormal);
	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gNormal, 0);

	// Albedo
	glGenTextures(1, &gAlbedo);
	glBindTexture(GL_TEXTURE_2D, gAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gAlbedo, 0);

	// Material: R = metallic, G = roughness, B = AO
	glGenTextures(1, &gMaterial);
	glBindTexture(GL_TEXTURE_2D, gMaterial);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gMaterial, 0);

	// Depth
	glGenTextures(1, &gDepth);
	glBindTexture(GL_TEXTURE_2D, gDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, framebufferWidth, framebufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);

	unsigned int attachments[3] =
	{
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2
	};

	glDrawBuffers(3, attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "G-Buffer framebuffer is not complete!\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


glm::mat4 Renderer::renderShadowPass(const Scene& scene, const DirectionalLight& light)
{
	glm::vec3 lightPos = -light.direction * 20.0f;

	glm::mat4 lightProjection = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
	glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 lightSpaceMatrix = lightProjection * lightView;

	glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
	glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
	glClear(GL_DEPTH_BUFFER_BIT);

	shadowShader.use();
	shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

	for (const auto& object : scene.getObjects())
	{
		if (object->model == nullptr)
		{
			continue;
		}

		glm::mat4 model = object->transform.getMatrix();

		shadowShader.setMat4("model", model);
		object->model->draw(shadowShader);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	return lightSpaceMatrix;
}


void Renderer::renderGeometryPass(const Scene& scene, Camera& camera)
{
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glEnable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	geometryShader.use();

	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight), 0.1f, 100.0f);

	geometryShader.setMat4("view", view);
	geometryShader.setMat4("projection", projection);

	for (const auto& object : scene.getObjects())
	{
		if (object->model == nullptr)
		{
			continue;
		}

		glm::mat4 model = object->transform.getMatrix();

		geometryShader.setMat4("model", model);
		object->model->draw(geometryShader);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::renderDeferredLightingPass(Mesh& quad, Camera& camera, const DirectionalLight& directionalLight, const PointLight* pointLights, int pointLightCount, const glm::mat4& lightSpaceMatrix, int debugView)
{
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	deferredShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gNormal);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gAlbedo);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gMaterial);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, gDepth);

	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, depthMap);

	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight), 0.1f, 100.0f);

	glm::mat4 inverseViewProjection = glm::inverse(projection * view);

	deferredShader.setMat4("inverseViewProjection", inverseViewProjection);
	deferredShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
	deferredShader.setVec3("viewPos", camera.Position);

	deferredShader.setVec3("directionalLight.direction", directionalLight.direction);
	deferredShader.setVec3("directionalLight.color", directionalLight.color);
	deferredShader.setFloat("directionalLight.intensity", directionalLight.intensity);
	deferredShader.setInt("debugView", debugView);

	int count = std::min(pointLightCount, 4);
	deferredShader.setInt("pointLightCount", count);

	for (int i = 0; i < count; ++i)
	{
		std::string base = "pointLights[" + std::to_string(i) + "]";

		deferredShader.setVec3(base + ".position", pointLights[i].position);
		deferredShader.setVec3(base + ".color", pointLights[i].color);
		deferredShader.setFloat(base + ".intensity", pointLights[i].intensity);
		deferredShader.setFloat(base + ".constant", pointLights[i].constant);
		deferredShader.setFloat(base + ".linear", pointLights[i].linear);
		deferredShader.setFloat(base + ".quadratic", pointLights[i].quadratic);
	}

	quad.draw();

	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::renderScenePass(const Scene& scene, Camera& camera, const DirectionalLight& directionalLight, const PointLight* pointLights, int pointLightCount, const glm::mat4& lightSpaceMatrix)
{
	beginScenePass();

	sceneShader.use();

	sceneShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, depthMap);

	sceneShader.setVec3("directionalLight.direction", directionalLight.direction);
	sceneShader.setVec3("directionalLight.color", directionalLight.color);
	sceneShader.setFloat("directionalLight.intensity", directionalLight.intensity);

	sceneShader.setInt("pointLightCount", pointLightCount);

	for (int i = 0; i < pointLightCount; ++i)
	{
		std::string base = "pointLights[" + std::to_string(i) + "]";

		sceneShader.setVec3((base + ".position").c_str(), pointLights[i].position);
		sceneShader.setVec3((base + ".color").c_str(), pointLights[i].color);
		sceneShader.setFloat((base + ".intensity").c_str(), pointLights[i].intensity);
		sceneShader.setFloat(base + ".constant", pointLights[i].constant);
		sceneShader.setFloat(base + ".linear", pointLights[i].linear);
		sceneShader.setFloat(base + ".quadratic", pointLights[i].quadratic);
	}

	sceneShader.setVec3("viewPos", camera.Position);

	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight), 0.1f, 100.0f);

	sceneShader.setMat4("view", view);
	sceneShader.setMat4("projection", projection);

	for (const auto& object : scene.getObjects())
	{
		if (object->model == nullptr)
		{
			continue;
		}

		glm::mat4 model = object->transform.getMatrix();

		sceneShader.setMat4("model", model);
		object->model->draw(sceneShader);
	}
}


void Renderer::renderScreenPass(Mesh& quad, int toneMappingMode, int debugView)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	screenShader.use();
	screenShader.setInt("debugView", debugView);
	screenShader.setInt("toneMappingMode", toneMappingMode);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sceneColorTexture);

	quad.draw();

	glEnable(GL_DEPTH_TEST);
}


unsigned int Renderer::getShadowMap() const
{
	return depthMap;
}


void Renderer::resizeSceneFramebuffer(int width, int height)
{
	framebufferWidth = width;
	framebufferHeight = height;

	glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

	glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebufferWidth, framebufferHeight);

	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, gAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, gMaterial);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, gDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, framebufferWidth, framebufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
}


void Renderer::beginScenePass()
{
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


unsigned int Renderer::getSceneColorTexture() const
{
	return sceneColorTexture;
}