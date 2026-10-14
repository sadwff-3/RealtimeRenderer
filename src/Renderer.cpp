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
#include<random>
#include<cmath>
#include<cstdlib>
#include<functional>


namespace
{
	float halton(unsigned int index, unsigned int base)
	{
		float result = 0.0f;
		float fraction = 1.0f;

		while (index > 0)
		{
			fraction /= static_cast<float>(base);
			result += fraction * static_cast<float>(index % base);
			index /= base;
		}

		return result;
	}
}


Renderer::Renderer(int width, int height) :shadowShader("shaders/shadow_depth.vert", "shaders/shadow_depth.frag"), pointShadowShader("shaders/point_shadow.vert", "shaders/point_shadow.frag"), sceneShader("shaders/pbr.vert", "shaders/pbr.frag"), screenShader("shaders/screen.vert", "shaders/screen.frag"), geometryShader("shaders/gbuffer.vert", "shaders/gbuffer.frag"), ssaoShader("shaders/screen.vert", "shaders/ssao.frag"), ssaoBlurShader("shaders/screen.vert", "shaders/ssao_blur.frag"), deferredShader("shaders/screen.vert", "shaders/deferred_lighting.frag"), taaShader("shaders/screen.vert", "shaders/taa_resolve.frag"), bloomDownsampleShader("shaders/screen.vert", "shaders/bloom_downsample.frag"), bloomUpsampleShader("shaders/screen.vert", "shaders/bloom_upsample.frag"), framebufferWidth(width), framebufferHeight(height)
{
	initSceneFramebuffer();
	initFinalFramebuffer();
	initTAAFramebuffers();
	initGBuffer();
	initSSAOFramebuffer();
	initSSAOKernel();
	initSSAONoiseTexture();
	initSSAOBlur();
	initDirectionalShadowMaps();
	initSpotShadowMaps();
	initPointShadowMaps();
	initBloomFramebuffer();

	ssaoShader.use();
	ssaoShader.setInt("gDepth", 0);
	ssaoShader.setInt("gNormal", 1);
	ssaoShader.setInt("noiseTexture", 2);

	ssaoBlurShader.use();
	ssaoBlurShader.setInt("ssaoInput", 0);

	deferredShader.use();
	deferredShader.setInt("gNormal", 0);
	deferredShader.setInt("gAlbedo", 1);
	deferredShader.setInt("gMaterial", 2);
	deferredShader.setInt("gDepth", 3);
	deferredShader.setInt("directionalShadowMaps", 4);
	deferredShader.setInt("ssaoMap", 5);
	deferredShader.setInt("spotShadowMaps", 6);
	deferredShader.setInt("pointShadowMaps", 7);
	deferredShader.setInt("environmentMap", 8);
	deferredShader.setInt("irradianceMap", 9);
	deferredShader.setInt("prefilterMap", 10);
	deferredShader.setInt("brdfLUT", 11);
	deferredShader.setInt("gVelocity", 12);
	deferredShader.setInt("gEmission", 13);

	taaShader.use();
	taaShader.setInt("currentColor", 0);
	taaShader.setInt("historyColor", 1);
	taaShader.setInt("gVelocity", 2);
	taaShader.setInt("gDepth", 3);
	
	screenShader.use();
	screenShader.setInt("screenTexture", 0);
	screenShader.setInt("bloomTexture", 1);
	screenShader.setFloat("exposure", 1.0f);
}


void Renderer::beginTAAFrame(const Camera& camera)
{
	if (framebufferWidth <= 0 || framebufferHeight <= 0)
		return;

	if (hasPreviousTAAFrame)
		previousViewProjection = frameProjection * frameView;

	float aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

	frameView = camera.getViewMatrix();
	frameProjection = camera.getProjectionMatrix(aspectRatio);

	previousTAAJitterPixels = taaJitterPixels;
	taaJitterPixels = glm::vec2(0.0f);

	if (taaEnabled)
	{
		unsigned int sampleIndex = (taaFrameIndex % 8) + 1;

		taaJitterPixels.x = halton(sampleIndex, 2) - 0.5f;
		taaJitterPixels.y = halton(sampleIndex, 3) - 0.5f;
		static const bool disableJitter = std::getenv("TAA_DISABLE_JITTER") != nullptr;
		if (disableJitter) taaJitterPixels = glm::vec2(0.0f);

		glm::mat4 jitterMatrix(1.0f);
		jitterMatrix[3][0] = 2.0f * taaJitterPixels.x / static_cast<float>(framebufferWidth);
		jitterMatrix[3][1] = 2.0f * taaJitterPixels.y / static_cast<float>(framebufferHeight);

		frameProjection = jitterMatrix * frameProjection;

		++taaFrameIndex;
	}

	if (!hasPreviousTAAFrame)
	{
		previousViewProjection = frameProjection * frameView;
		previousTAAJitterPixels = taaJitterPixels;
		hasPreviousTAAFrame = true;
	}
}


Renderer::~Renderer()
{
	destroyBloomFramebuffer();

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

	if (ssaoTexture != 0)
	{
		glDeleteTextures(1, &ssaoTexture);
	}
	if (ssaoFBO != 0)
	{
		glDeleteFramebuffers(1, &ssaoFBO);
	}

	if (gBuffer != 0)
	{
		glDeleteFramebuffers(1, &gBuffer);
	}

	if (ssaoBlurTexture != 0)
	{
		glDeleteTextures(1, &ssaoBlurTexture);
	}

	if (ssaoBlurFBO != 0)
	{
		glDeleteFramebuffers(1, &ssaoBlurFBO);
	}

	if (spotShadowArray != 0)
	{
		glDeleteTextures(1, &spotShadowArray);
	}

	if (spotShadowFBO != 0)
	{
		glDeleteFramebuffers(1, &spotShadowFBO);
	}
	
	if (pointShadowArray != 0)
	{
		glDeleteTextures(1, &pointShadowArray);
	}

	if (pointShadowFBO != 0)
	{
		glDeleteFramebuffers(1, &pointShadowFBO);
	}

	if (finalColorTexture != 0)
	{
		glDeleteTextures(1, &finalColorTexture);
	}

	if (finalFBO != 0)
	{
		glDeleteFramebuffers(1, &finalFBO);
	}

	if (gObjectID != 0)
	{
		glDeleteTextures(1, &gObjectID);
	}

	if (gMaterialID != 0)
	{
		glDeleteTextures(1, &gMaterialID);
	}

	if (gVelocity != 0)
	{
		glDeleteTextures(1, &gVelocity);
	}

	if (gEmission != 0)
	{
		glDeleteTextures(1, &gEmission);
	}

	glDeleteTextures(2, taaHistoryColor.data());
	glDeleteFramebuffers(2, taaHistoryFBO.data());
}


void Renderer::initDirectionalShadowMaps()
{
	glGenFramebuffers(1, &directionalShadowFBO);
	glGenTextures(1, &directionalShadowArray);

	glBindTexture(GL_TEXTURE_2D_ARRAY, directionalShadowArray);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, DIRECTIONAL_SHADOW_SIZE, DIRECTIONAL_SHADOW_SIZE, MAX_DIRECTIONAL_SHADOWS * CASCADE_COUNT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	const float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);

	glBindFramebuffer(GL_FRAMEBUFFER, directionalShadowFBO);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, directionalShadowArray, 0, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}


void Renderer::initSpotShadowMaps()
{
	glGenFramebuffers(1, &spotShadowFBO);
	glGenTextures(1, &spotShadowArray);

	glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowArray);

	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SPOT_SHADOW_SIZE, SPOT_SHADOW_SIZE, MAX_SPOT_SHADOWS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	const float border[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, border);

	glBindFramebuffer(GL_FRAMEBUFFER, spotShadowFBO);
	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, spotShadowArray, 0, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

}


void Renderer::initPointShadowMaps()
{
	glGenFramebuffers(1, &pointShadowFBO);

	glGenTextures(1, &pointShadowArray);

	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowArray);

	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0 ,GL_DEPTH_COMPONENT24, POINT_SHADOW_SIZE, POINT_SHADOW_SIZE, 6 * MAX_POINT_SHADOWS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO);

	glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, pointShadowArray, 0, 0);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cerr << "Point shadow framebuffer is incomplete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
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


void Renderer::initFinalFramebuffer()
{
	glGenFramebuffers(1, &finalFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, finalFBO);

	glGenTextures(1, &finalColorTexture);
	glBindTexture(GL_TEXTURE_2D, finalColorTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, finalColorTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Final framebuffer is not complete!\n";
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

	//ObjectID
	glGenTextures(1, &gObjectID);
	glBindTexture(GL_TEXTURE_2D, gObjectID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, framebufferWidth, framebufferHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, gObjectID, 0);

	//MaterialID
	glGenTextures(1, &gMaterialID);
	glBindTexture(GL_TEXTURE_2D, gMaterialID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, framebufferWidth, framebufferHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, GL_TEXTURE_2D, gMaterialID, 0);

	//Depth
	glGenTextures(1, &gDepth);
	glBindTexture(GL_TEXTURE_2D, gDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, framebufferWidth, framebufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);

	//Velocity
	glGenTextures(1, &gVelocity);
	glBindTexture(GL_TEXTURE_2D, gVelocity);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, framebufferWidth, framebufferHeight, 0, GL_RG, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, GL_TEXTURE_2D, gVelocity, 0);

	//Emission
	glGenTextures(1, &gEmission);
	glBindTexture(GL_TEXTURE_2D, gEmission);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, GL_TEXTURE_2D, gEmission, 0);

	unsigned int attachments[7] =
	{
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3,
		GL_COLOR_ATTACHMENT4,
		GL_COLOR_ATTACHMENT5,
		GL_COLOR_ATTACHMENT6
	};

	glDrawBuffers(7, attachments);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "G-Buffer framebuffer is not complete!\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::initSSAOFramebuffer()
{
	glGenFramebuffers(1, &ssaoFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

	glGenTextures(1, &ssaoTexture);
	glBindTexture(GL_TEXTURE_2D, ssaoTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, framebufferWidth, framebufferHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "SSAO framebuffer is not complete!\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::initSSAOBlur()
{
	glGenFramebuffers(1, &ssaoBlurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);

	glGenTextures(1, &ssaoBlurTexture);
	glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, framebufferWidth, framebufferHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "SSAO Blur framebuffer is not complete!\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::initSSAOKernel()
{
	std::mt19937 rng(42);
	std::uniform_real_distribution<float> random01(0.0f, 1.0f);

	for (size_t i = 0; i < ssaoKernel.size(); ++i)
	{
		glm::vec3 sample(random01(rng) * 2.0f - 1.0f, random01(rng) * 2.0f - 1.0f, random01(rng));

		sample = glm::normalize(sample);
		sample *= random01(rng);

		float t = static_cast<float>(i) / ssaoKernel.size();
		sample *= glm::mix(0.1f, 1.0f, t * t);

		ssaoKernel[i] = sample;

	}

}


void Renderer::initSSAONoiseTexture()
{
	std::mt19937 rng(1337);
	std::uniform_real_distribution<float> random(-1.0f, 1.0f);

	std::array<float, 4 * 4 * 3> noise{};

	for (size_t i = 0; i < 16; ++i)
	{
		noise[i * 3] = random(rng);
		noise[i * 3 + 1] = random(rng);
		noise[i * 3 + 2] = random(rng);
	}

	glGenTextures(1, &ssaoNoiseTexture);
	glBindTexture(GL_TEXTURE_2D, ssaoNoiseTexture);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBindTexture(GL_TEXTURE_2D, 0);
}


void Renderer::initTAAFramebuffers()
{
	glGenFramebuffers(2, taaHistoryFBO.data());
	glGenTextures(2, taaHistoryColor.data());

	for (int i = 0; i < 2; ++i)
	{
		glBindTexture(GL_TEXTURE_2D, taaHistoryColor[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, taaHistoryFBO[i]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, taaHistoryColor[i], 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cerr << "TAA framebuffer " << i << " is incomplete!\n";
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
}


void Renderer::initBloomFramebuffer()
{
	if (framebufferWidth <= 0 || framebufferHeight <= 0)
		return;

	glGenFramebuffers(1, &bloomFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);

	int width = framebufferWidth;
	int height = framebufferHeight;

	for (int level = 0; level < 6; ++level)
	{
		width = std::max(1, width / 2);
		height = std::max(1, height / 2);

		BloomMip mip{ 0, width, height };

		glGenTextures(1, &mip.texture);
		glBindTexture(GL_TEXTURE_2D, mip.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		bloomMips.push_back(mip);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cerr << "Bloom framebuffer is incomplete!\n";
			destroyBloomFramebuffer();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return;
		}

		if (width == 1 && height == 1)
			break;
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::renderDirectionalShadowPass(const Scene& scene, const Camera& camera)
{
	float aspectRatio = static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);

	const float shadowFar = std::min(camera.FarPlane, DIRECTIONAL_SHADOW_DISTANCE);
	cascadeSplits = calculateCascadeSplits(camera.NearPlane, shadowFar);

	glViewport(0, 0, DIRECTIONAL_SHADOW_SIZE, DIRECTIONAL_SHADOW_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, directionalShadowFBO);

	shadowShader.use();

	const GLboolean previousPolygonOffset = glIsEnabled(GL_POLYGON_OFFSET_FILL);
	GLfloat previousOffsetFactor = 0.0f;
	GLfloat previousOffsetUnits = 0.0f;
	glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &previousOffsetFactor);
	glGetFloatv(GL_POLYGON_OFFSET_UNITS, &previousOffsetUnits);
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(2.0f, 4.0f);

	directionalShadowCount = 0;

	for (const SceneObject* lightObject : scene.getLights())
	{
		if (!lightObject->light.has_value())
			continue;

		const Light& light = lightObject->light.value();

		if (light.type != LightType::Directional)
			continue;

		if (!light.castShadow)
			continue;

		if (directionalShadowCount >= MAX_DIRECTIONAL_SHADOWS)
			break;

		glm::vec3 direction = lightObject->transform.getForward();

		for (int cascade = 0; cascade < CASCADE_COUNT; ++cascade)
		{
			float cascadeNear = cascade == 0 ? camera.NearPlane : cascadeSplits[cascade - 1];
			float cascadeFar = cascadeSplits[cascade];
			
			glm::mat4 lightSpaceMatrix = calculateCascadeLightMatrix(direction, camera, aspectRatio, cascadeNear, cascadeFar);
			
			glm::vec4 shadowOrigin = lightSpaceMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
			glm::vec2 texelOrigin = glm::vec2(shadowOrigin) * (DIRECTIONAL_SHADOW_SIZE * 0.5f);
			glm::vec2 offset = (glm::round(texelOrigin) - texelOrigin) * (2.0f / DIRECTIONAL_SHADOW_SIZE);
			lightSpaceMatrix[3][0] += offset.x;
			lightSpaceMatrix[3][1] += offset.y;

			int layer = directionalShadowCount * CASCADE_COUNT + cascade;
			directionalCascadeMatrices[layer] = lightSpaceMatrix;

			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, directionalShadowArray, 0, layer);

			glClear(GL_DEPTH_BUFFER_BIT);

			shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

			for (const auto& object : scene.getObjects())
			{
				if (object->model == nullptr)
					continue;

				glm::mat4 model = object->transform.getMatrix();
				glm::mat4 localToShadowClip = lightSpaceMatrix * model;
				float clipMargin = 4.0f / static_cast<float>(DIRECTIONAL_SHADOW_SIZE);

				shadowShader.setMat4("model", model);
				object->model->drawShadowCulled(shadowShader, localToShadowClip, clipMargin);
			}
		}

		++directionalShadowCount;
	}

	glPolygonOffset(previousOffsetFactor, previousOffsetUnits);
	if (!previousPolygonOffset)
		glDisable(GL_POLYGON_OFFSET_FILL);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
}


void Renderer::renderSpotShadowPass(const Scene& scene)
{
	glViewport(0, 0, SPOT_SHADOW_SIZE, SPOT_SHADOW_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, spotShadowFBO);

	shadowShader.use();

	spotShadowCount = 0;
	for (const SceneObject* lightObject : scene.getLights())
	{
		if (!lightObject->light.has_value())
			continue;

		const Light& light = lightObject->light.value();

		if (light.type != LightType::Spot)
			continue;

		if (!light.castShadow)
			continue;

		if (spotShadowCount >= MAX_SPOT_SHADOWS)
			break;

		glm::vec3 position = lightObject->transform.position;
		glm::vec3 direction = lightObject->transform.getForward();
		
		glm::vec3 up(0.0f, 1.0f, 0.0f);

		if (std::abs(glm::dot(direction, up)) > 0.99f)
			up = glm::vec3(0.0f, 0.0f, 1.0f);

		glm::mat4 lightProjection = glm::perspective(glm::radians(light.outerAngle * 2.0f), 1.0f, 0.1f, light.range);

		glm::mat4 lightView = glm::lookAt(position, position + direction, up);

		glm::mat4 lightSpaceMatrix = lightProjection * lightView;

		spotLightSpaceMatrices[spotShadowCount] = lightSpaceMatrix;

		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, spotShadowArray, 0, spotShadowCount);

		glClear(GL_DEPTH_BUFFER_BIT);

		shadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

		for (const auto& object : scene.getObjects())
		{
			if (object->model == nullptr)
				continue;

			glm::mat4 model = object->transform.getMatrix();

			shadowShader.setMat4("model", model);
			object->model->drawShadow(shadowShader);
		}

		++spotShadowCount;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
}


void Renderer::renderPointShadowPass(const Scene& scene)
{
	pointShadowCount = 0;
	const int shadowBudget = std::clamp(pointShadowBudget, 0, MAX_POINT_SHADOWS);
	if (shadowBudget == 0) return;
	int activePointCount = 0;

	glViewport(0, 0, POINT_SHADOW_SIZE, POINT_SHADOW_SIZE);
	glBindFramebuffer(GL_FRAMEBUFFER, pointShadowFBO);

	pointShadowShader.use();

	for (const SceneObject* object : scene.getLights())
	{
		if (!object->light.has_value())
			continue;

		const Light& light = object->light.value();

		if (light.type != LightType::Point || light.intensity <= 0.0f)
			continue;

		if (++activePointCount > 4) break;

		if (!light.castShadow)
			continue;

		if (pointShadowCount >= shadowBudget)
			break;

		glm::vec3 lightPos = object->transform.position;
		float farPlane = light.range;

		pointShadowShader.setVec3("lightPos", lightPos);
		pointShadowShader.setFloat("farPlane", farPlane);

		glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);

		glm::mat4 shadowViews[6] =
		{
			glm::lookAt(lightPos, lightPos + glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(lightPos, lightPos + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			glm::lookAt(lightPos, lightPos + glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		};

		for (int face = 0; face < 6; ++face)
		{
			int layer = pointShadowCount * 6 + face;

			glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, pointShadowArray, 0, layer);

			glClear(GL_DEPTH_BUFFER_BIT);

			glm::mat4 lightSpaceMatrix = projection * shadowViews[face];

			pointShadowShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

			for (const auto& object : scene.getObjects())
			{
				if (object->model == nullptr)
					continue;

				glm::mat4 model = object->transform.getMatrix();

				pointShadowShader.setMat4("model", model);
				object->model->drawShadowCulled(pointShadowShader, lightSpaceMatrix * model, 4.0f / static_cast<float>(POINT_SHADOW_SIZE));
			}
		}

		++pointShadowCount;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
}


void Renderer::renderGeometryPass(const Scene& scene, Camera& camera)
{
	glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	const float clearColor[4] = { 0.0f,0.0f,0.0f,0.0f };

	const unsigned int clearObjectID[1] = { 0 };
	const unsigned int clearMaterialID[1] = { 0 };

	glClearBufferfv(GL_COLOR, 0, clearColor);
	glClearBufferfv(GL_COLOR, 1, clearColor);
	glClearBufferfv(GL_COLOR, 2, clearColor);
	glClearBufferuiv(GL_COLOR, 3, clearObjectID);
	glClearBufferuiv(GL_COLOR, 4, clearMaterialID);
	const float clearVelocity[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glClearBufferfv(GL_COLOR, 5, clearVelocity);
	glClearBufferfv(GL_COLOR, 6, clearColor);

	geometryShader.use();

	glm::mat4 view = frameView;
	glm::mat4 projection = frameProjection;

	geometryShader.setMat4("view", view);
	geometryShader.setMat4("projection", projection);

	const auto& objects = scene.getObjects();

	std::unordered_map<const SceneObject*, glm::mat4> currentModelMatrices;
	currentModelMatrices.reserve(objects.size());

	// Only bypass rejection when every tracked image-producing scene input is unchanged.
	std::size_t signature = 0;
	std::size_t appearance = 0;
	auto add = [&](auto value) { signature ^= std::hash<decltype(value)>{}(value) + 0x9e3779b9 + (signature << 6) + (signature >> 2); };
	auto addAppearance = [&](auto value) { add(value); appearance ^= std::hash<decltype(value)>{}(value) + 0x9e3779b9 + (appearance << 6) + (appearance >> 2); };
	auto addMatrix = [&](const glm::mat4& matrix) { for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) add(matrix[c][r]); };
	addAppearance(std::clamp(pointShadowBudget, 0, MAX_POINT_SHADOWS));
	addMatrix(camera.getProjectionMatrix(static_cast<float>(framebufferWidth) / framebufferHeight) * frameView);
	for (const auto& object : objects)
	{
		addAppearance(object.get());
		addAppearance(object->model);
		addMatrix(object->transform.getMatrix());
		if (object->light)
		{
			const auto& light = *object->light;
			glm::mat4 lightTransform = object->transform.getMatrix();
			for (int c = 0; c < 4; ++c) for (int r = 0; r < 4; ++r) addAppearance(lightTransform[c][r]);
			addAppearance(static_cast<int>(light.type));
			for (int c = 0; c < 3; ++c) addAppearance(light.color[c]);
			addAppearance(light.intensity); addAppearance(light.range); addAppearance(light.innerAngle); addAppearance(light.outerAngle); addAppearance(light.castShadow);
		}
		if (!object->model) continue;
		for (std::size_t i = 0; i < object->model->getMaterialCount(); ++i)
		{
			const auto& material = object->model->getMaterial(i);
			addAppearance(material.albedo); addAppearance(material.normalMap); addAppearance(material.metallicMap); addAppearance(material.roughnessMap); addAppearance(material.aoMap);
			addAppearance(material.emissiveMap);
			addAppearance(material.emissiveStrength);
			for (int c = 0; c < 3; ++c)
				addAppearance(material.emissiveFactor[c]);
			for (int c = 0; c < 4; ++c) addAppearance(material.baseColorFactor[c]);
			addAppearance(material.metallicFactor); addAppearance(material.roughnessFactor); addAppearance(material.normalYSign); addAppearance(material.alphaMode); addAppearance(material.alphaCutoff); addAppearance(material.doubleSided);
		}
	}
	bool sceneUnchanged = hasTAASceneSignature && signature == taaSceneSignature;
	taaStableFrames = sceneUnchanged && taaHistoryValid ? std::min(taaStableFrames + 1, 4095) : 0;
	if (hasTAASceneSignature && appearance != taaAppearanceSignature) taaHistoryValid = false;
	taaSceneSignature = signature;
	taaAppearanceSignature = appearance;
	hasTAASceneSignature = true;

	for (size_t i = 0; i < objects.size(); ++i)
	{
		const auto& object = objects[i];

		if (object->model == nullptr)
		{
			continue;
		}

		glm::mat4 model = object->transform.getMatrix();

		const SceneObject* key = object.get();
		auto it = previousModelMatrices.find(key);
		bool existedLastFrame = it != previousModelMatrices.end();

		glm::mat4 previousModel = existedLastFrame ? it->second : model;
		glm::mat4 previousVP = existedLastFrame ? previousViewProjection : frameProjection * frameView;

		geometryShader.setMat4("model", model);
		geometryShader.setMat4("previousModel", previousModel);
		geometryShader.setMat4("previousViewProjection", previousVP);
		geometryShader.setInt("objectID", static_cast<int>(i + 1));

		object->model->draw(geometryShader);
		
		currentModelMatrices.emplace(key, model);
	}

	bool sceneMembershipChanged = currentModelMatrices.size() != previousModelMatrices.size();
	for (const auto& entry : currentModelMatrices)
		sceneMembershipChanged = sceneMembershipChanged || previousModelMatrices.find(entry.first) == previousModelMatrices.end();
	if (sceneMembershipChanged)
		taaHistoryValid = false;
	previousModelMatrices.swap(currentModelMatrices);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::renderSSAOPass(Mesh& quad, Camera& camera)
{
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glDisable(GL_DEPTH_TEST);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	ssaoShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gDepth);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gNormal);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ssaoNoiseTexture);

	glm::mat4 view = frameView;
	glm::mat4 projection = frameProjection;

	ssaoShader.setMat4("view", view);
	ssaoShader.setMat4("projection", projection);
	ssaoShader.setMat4("inverseProjection", glm::inverse(projection));

	for (size_t i = 0; i < ssaoKernel.size(); ++i)
	{
		ssaoShader.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
	}

	quad.draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}


void Renderer::renderSSAOBlurPass(Mesh& quad)
{
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glDisable(GL_DEPTH_TEST);
	glClear(GL_COLOR_BUFFER_BIT);

	ssaoBlurShader.use();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ssaoTexture);

	quad.draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}


void Renderer::renderDeferredLightingPass(Mesh& quad, Camera& camera, const Scene& scene, int debugView, unsigned int environmentCubemap, unsigned int irradianceMap, unsigned int prefilterMap, unsigned int brdfLUT)
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
	glBindTexture(GL_TEXTURE_2D_ARRAY, directionalShadowArray);

	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);

	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D_ARRAY, spotShadowArray);

	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, pointShadowArray);

	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_CUBE_MAP, environmentCubemap);

	glActiveTexture(GL_TEXTURE9);
	glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);

	glActiveTexture(GL_TEXTURE10);
	glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);

	glActiveTexture(GL_TEXTURE11);
	glBindTexture(GL_TEXTURE_2D, brdfLUT);

	glActiveTexture(GL_TEXTURE12);
	glBindTexture(GL_TEXTURE_2D, gVelocity);

	glActiveTexture(GL_TEXTURE13);
	glBindTexture(GL_TEXTURE_2D, gEmission);


	glm::mat4 view = frameView;
	glm::mat4 projection = frameProjection;

	glm::mat4 inverseViewProjection = glm::inverse(projection * view);

	deferredShader.setMat4("inverseViewProjection", inverseViewProjection);
	deferredShader.setVec3("viewPos", camera.Position);
	deferredShader.setMat4("csmView", view);

	for (int i = 0; i < CASCADE_COUNT; ++i)
	{
		deferredShader.setFloat("cascadeSplits[" + std::to_string(i) + "]", cascadeSplits[i]);
	}

	for (int i = 0; i < MAX_DIRECTIONAL_SHADOWS * CASCADE_COUNT; ++i)
	{
		deferredShader.setMat4("cascadeLightSpaceMatrices[" + std::to_string(i) + "]", directionalCascadeMatrices[i]);
	}

	
	deferredShader.setInt("debugView", debugView);


	int directionalCount = 0;
	int directionalShadowIndex = 0;

	for (const SceneObject* object : scene.getLights())
	{
		if (!object->light.has_value())
			continue;

		const Light& light = object->light.value();

		if (light.type != LightType::Directional)
			continue;

		if (directionalCount >= MAX_DIRECTIONAL_SHADOWS)
			break;

		std::string base =
			"directionalLights[" + std::to_string(directionalCount) + "]";

		deferredShader.setVec3(base + ".direction", object->transform.getForward());
		deferredShader.setVec3(base + ".color", light.color);
		deferredShader.setFloat(base + ".intensity", light.intensity);

		int shadowLayer = -1;

		if (light.castShadow && directionalShadowIndex < directionalShadowCount)
		{
			shadowLayer = directionalShadowIndex;

			++directionalShadowIndex;
		}

		deferredShader.setInt(base + ".shadowLayer", shadowLayer);

		++directionalCount;
	}

	deferredShader.setInt("directionalLightCount", directionalCount);


	int count = 0;
	int pointShadowIndex = 0;
	for (const auto& object : scene.getLights())
	{
		if (!object->light.has_value())
			continue;

		const Light& light = object->light.value();

		if (light.type != LightType::Point || light.intensity <= 0.0f)
			continue;

		if (count >= 4)
			break;

		std::string base = "pointLights[" + std::to_string(count) + "]";

		deferredShader.setVec3(base + ".position", object->transform.position);
		deferredShader.setVec3(base + ".color", light.color);
		deferredShader.setFloat(base + ".intensity", light.intensity);

		deferredShader.setFloat(base + ".constant", 1.0f);
		deferredShader.setFloat(base + ".linear", 0.09f);
		deferredShader.setFloat(base + ".quadratic", 0.032f);

		int shadowLayer = -1;

		if (light.castShadow && pointShadowIndex < pointShadowCount)
		{
			shadowLayer = pointShadowIndex;
			++pointShadowIndex;
		}

		deferredShader.setInt(base + ".shadowLayer", shadowLayer);
		deferredShader.setFloat(base + ".farPlane", light.range);

		++count;
	}
	deferredShader.setInt("pointLightCount", count);


	int spotCount = 0;
	int shadowIndex = 0;
	for (const SceneObject* object : scene.getLights())
	{
		if (!object->light.has_value())
			continue;

		const Light& light = object->light.value();
		if (light.type != LightType::Spot)
			continue;

		if (spotCount >= 10)
			break;

		std::string base = "spotLights[" + std::to_string(spotCount) + "]";

		deferredShader.setVec3(base + ".position", object->transform.position);
		deferredShader.setVec3(base + ".direction", object->transform.getForward());
		deferredShader.setVec3(base + ".color", light.color);
		deferredShader.setFloat(base + ".intensity", light.intensity);
		deferredShader.setFloat(base + ".range", light.range);

		float inner = glm::cos(glm::radians(light.innerAngle));
		float outer = glm::cos(glm::radians(light.outerAngle));

		deferredShader.setFloat(base + ".innerCutoff", inner);
		deferredShader.setFloat(base + ".outerCutoff", outer);

		int layer = -1;

		if (light.castShadow && shadowIndex < spotShadowCount)
		{
			layer = shadowIndex;

			deferredShader.setMat4(base + ".lightSpaceMatrix", spotLightSpaceMatrices[shadowIndex]);

			++shadowIndex;
		}
		

		deferredShader.setInt(base + ".shadowLayer", layer);

		++spotCount;
	}

	deferredShader.setInt("spotLightCount", spotCount);

	quad.draw();

	glEnable(GL_DEPTH_TEST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void Renderer::renderTAAPass(Mesh& quad, int debugView)
{
	if (!taaEnabled || debugView != 0)
	{
		taaHistoryValid = false;
		return;
	}

	int writeIndex = 1 - taaHistoryReadIndex;

	glBindFramebuffer(GL_FRAMEBUFFER, taaHistoryFBO[writeIndex]);
	glViewport(0, 0, framebufferWidth, framebufferHeight);
	glDisable(GL_DEPTH_TEST);

	taaShader.use();
	taaShader.setInt("historyValid", taaHistoryValid ? 1 : 0);
	taaShader.setInt("stableFrames", taaStableFrames);
	taaShader.setFloat("currentJitterX", taaJitterPixels.x / static_cast<float>(framebufferWidth));
	taaShader.setFloat("currentJitterY", taaJitterPixels.y / static_cast<float>(framebufferHeight));
	taaShader.setFloat("previousJitterX", previousTAAJitterPixels.x / static_cast<float>(framebufferWidth));
	taaShader.setFloat("previousJitterY", previousTAAJitterPixels.y / static_cast<float>(framebufferHeight));
	taaShader.setMat4("inverseCurrentViewProjection", glm::inverse(frameProjection * frameView));
	taaShader.setMat4("previousViewProjection", previousViewProjection);
	taaShader.setVec3("currentCameraPosition", glm::vec3(glm::inverse(frameView)[3]));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, sceneColorTexture);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, taaHistoryColor[taaHistoryReadIndex]);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gVelocity);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, gDepth);

	static const bool diagnostics = std::getenv("TAA_DIAGNOSTICS") != nullptr;
	if (diagnostics && taaFrameIndex <= 16)
	{
		float velocity[2] = {};
		glGetTextureSubImage(gVelocity, 0, framebufferWidth / 2, framebufferHeight / 2, 0, 1, 1, 1, GL_RG, GL_FLOAT, sizeof(velocity), velocity);
		float depth = 1.0f;
		glGetTextureSubImage(gDepth, 0, framebufferWidth / 2, framebufferHeight / 2, 0, 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, sizeof(depth), &depth);
		glm::vec2 size(framebufferWidth, framebufferHeight);
		glm::vec2 uv = (glm::vec2(framebufferWidth / 2, framebufferHeight / 2) + 0.5f) / size;
		glm::vec2 previousUV = uv + (taaJitterPixels - previousTAAJitterPixels) / size - glm::vec2(velocity[0], velocity[1]);
		if (depth >= 1.0f)
		{
			glm::vec4 world = glm::inverse(frameProjection * frameView) * glm::vec4((uv + taaJitterPixels / size) * 2.0f - 1.0f, 1.0f, 1.0f);
			glm::vec4 previousDirection = previousViewProjection * glm::vec4(glm::vec3(world) / world.w - glm::vec3(glm::inverse(frameView)[3]), 0.0f);
			if (previousDirection.w > 0.0f) previousUV = glm::vec2(previousDirection) / previousDirection.w * 0.5f + 0.5f - previousTAAJitterPixels / size;
		}
		std::cout << "TAA sample=" << ((taaFrameIndex - 1) % 8 + 1) << " size=" << framebufferWidth << 'x' << framebufferHeight << " jitterPx=" << taaJitterPixels.x << ',' << taaJitterPixels.y << " previousJitterPx=" << previousTAAJitterPixels.x << ',' << previousTAAJitterPixels.y << " velocityPx=" << velocity[0] * framebufferWidth << ',' << velocity[1] * framebufferHeight << " previousUV=" << previousUV.x << ',' << previousUV.y << " stableFrames=" << taaStableFrames << " historyValid=" << taaHistoryValid << " read=" << taaHistoryReadIndex << " write=" << writeIndex << '\n';
	}

	quad.draw();

	taaHistoryReadIndex = writeIndex;
	taaHistoryValid = true;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glEnable(GL_DEPTH_TEST);
}


void Renderer::renderBloomPass(Mesh& quad, int debugView)
{
	bloomReady = false;

	if ((!bloom.enabled && debugView != 10) || (debugView != 0 && debugView != 10) || bloomMips.empty())
		return;

	const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
	const GLboolean blendEnabled = glIsEnabled(GL_BLEND);

	GLint drawFBO, readFBO;
	GLint viewport[4];
	GLint srcRGB, dstRGB, srcAlpha, dstAlpha;
	GLint eqRGB, eqAlpha;

	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFBO);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFBO);
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
	glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
	glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
	glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
	glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqRGB);
	glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &eqAlpha);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO);
	glActiveTexture(GL_TEXTURE0);

	unsigned int source = getPostTAAColorTexture(debugView);

	bloomDownsampleShader.use();
	bloomDownsampleShader.setInt("sourceTexture", 0);
	bloomDownsampleShader.setFloat("threshold", std::max(bloom.threshold, 0.0f));
	bloomDownsampleShader.setFloat("softKnee", std::clamp(bloom.softKnee, 0.0f, 1.0f));

	for (std::size_t i = 0; i < bloomMips.size(); ++i)
	{
		const auto& mip = bloomMips[i];

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mip.texture, 0);
		glViewport(0, 0, mip.width, mip.height);
		glBindTexture(GL_TEXTURE_2D, source);

		bloomDownsampleShader.setInt("firstPass", i == 0 ? 1 : 0);
		quad.draw();

		source = mip.texture;
	}

	bloomUpsampleShader.use();
	bloomUpsampleShader.setInt("sourceTexture", 0);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);

	for (int i = static_cast<int>(bloomMips.size()) - 1; i > 0; --i)
	{
		const auto& target = bloomMips[i - 1];

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
		glViewport(0, 0, target.width, target.height);
		glBindTexture(GL_TEXTURE_2D, bloomMips[i].texture);

		quad.draw();
	}

	bloomReady = true;

	glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
	glBlendEquationSeparate(eqRGB, eqAlpha);

	if (blendEnabled)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);

	if (depthEnabled)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, readFBO);
	glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
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


	sceneShader.setVec3("viewPos", camera.Position);

	glm::mat4 view = camera.getViewMatrix();
	glm::mat4 projection = camera.getProjectionMatrix(static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight));

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
	glBindFramebuffer(GL_FRAMEBUFFER, finalFBO);
	glViewport(0, 0, framebufferWidth, framebufferHeight);

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	screenShader.use();
	screenShader.setInt("debugView", debugView);
	screenShader.setInt("toneMappingMode", toneMappingMode);
	screenShader.setFloat("bloomIntensity", bloomReady && bloom.enabled ? std::max(bloom.intensity, 0.0f) : 0.0f);
	screenShader.setFloat("bloomScale", bloomReady ? 1.0f / static_cast<float>(bloomMips.size()) : 0.0f);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, bloomReady ? bloomMips.front().texture : 0);

	unsigned int outputTexture = taaEnabled && taaHistoryValid && debugView == 0 ? taaHistoryColor[taaHistoryReadIndex] : sceneColorTexture;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, outputTexture);

	quad.draw();

	glEnable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


unsigned int Renderer::getShadowMap() const
{
	return depthMap;
}


void Renderer::resizeSceneFramebuffer(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;
	if (width == framebufferWidth && height == framebufferHeight)
		return;

	framebufferWidth = width;
	framebufferHeight = height;

	destroyBloomFramebuffer();
	initBloomFramebuffer();

	glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, finalColorTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glBindRenderbuffer(GL_RENDERBUFFER, sceneRBO);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, framebufferWidth, framebufferHeight);

	glBindTexture(GL_TEXTURE_2D, gNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, gAlbedo);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, gMaterial);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, gObjectID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, framebufferWidth, framebufferHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

	glBindTexture(GL_TEXTURE_2D, gMaterialID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, framebufferWidth, framebufferHeight, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);

	glBindTexture(GL_TEXTURE_2D, gDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, framebufferWidth, framebufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, gVelocity);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, framebufferWidth, framebufferHeight, 0, GL_RG, GL_FLOAT, nullptr);
	hasPreviousTAAFrame = false;
	previousModelMatrices.clear();

	glBindTexture(GL_TEXTURE_2D, gEmission);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, framebufferWidth, framebufferHeight, 0, GL_RGB, GL_FLOAT, nullptr);

	glBindTexture(GL_TEXTURE_2D, ssaoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, framebufferWidth, framebufferHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, framebufferWidth, framebufferHeight, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, finalColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	for (int i = 0; i < 2; ++i)
	{
		glBindTexture(GL_TEXTURE_2D, taaHistoryColor[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, framebufferWidth, framebufferHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
	}

	taaHistoryReadIndex = 0;
	taaHistoryValid = false;
	hasPreviousTAAFrame = false;
	previousModelMatrices.clear();
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


unsigned int Renderer::getFinalColorTexture() const
{
	return finalColorTexture;
}


unsigned int Renderer::readObjectID(int x, int y) const
{
	if (x < 0 || y < 0 || x >= framebufferWidth || y >= framebufferHeight)
	{
		return 0;
	}

	unsigned int objectID = 0;
	GLint previousReadFramebuffer = 0;

	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
	glReadBuffer(GL_COLOR_ATTACHMENT3);
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &objectID);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);

	return objectID;
}


unsigned int Renderer::readMaterialID(int x, int y) const
{
	if (x < 0 || y < 0 || x >= framebufferWidth || y >= framebufferHeight)
	{
		return 0;
	}

	unsigned int materialID = 0;
	GLint previousReadFramebuffer = 0;

	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
	glReadBuffer(GL_COLOR_ATTACHMENT4);
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &materialID);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, previousReadFramebuffer);

	return materialID;
}


std::array<float,Renderer::CASCADE_COUNT> Renderer::calculateCascadeSplits(float nearPlane, float farPlane) const
{
	std::array<float, CASCADE_COUNT> splits{};

	const float lambda = 0.9f;

	for (int i = 0; i < CASCADE_COUNT; ++i)
	{
		float p = static_cast<float>(i + 1) / static_cast<float>(CASCADE_COUNT);

		float log = nearPlane * std::pow(farPlane / nearPlane, p);
		float uniform = nearPlane + (farPlane - nearPlane) * p;

		splits[i] = lambda * log + (1.0f - lambda) * uniform;
	}

	return splits;
}


std::vector<glm::vec4> Renderer::getFrustumCornersWorldSpace(const Camera& camera, float aspectRatio, float nearPlane, float farPlane) const
{
	float tanHalfFov = tan(glm::radians(camera.Zoom * 0.5f));

	float nearHeight = nearPlane * tanHalfFov;
	float nearWidth = nearHeight * aspectRatio;

	float farHeight = farPlane * tanHalfFov;
	float farWidth = farHeight * aspectRatio;

	glm::vec3 nearCenter = camera.Position + camera.Front * nearPlane;
	glm::vec3 farCenter = camera.Position + camera.Front * farPlane;

	glm::vec3 nearTopLeft = nearCenter + camera.Up * nearHeight - camera.Right * nearWidth;
	glm::vec3 nearTopRight = nearCenter + camera.Up * nearHeight + camera.Right * nearWidth;
	glm::vec3 nearBottomLeft = nearCenter - camera.Up * nearHeight - camera.Right * nearWidth;
	glm::vec3 nearBottomRight = nearCenter - camera.Up * nearHeight + camera.Right * nearWidth;
	glm::vec3 farTopLeft = farCenter + camera.Up * farHeight - camera.Right * farWidth;
	glm::vec3 farTopRight = farCenter + camera.Up * farHeight + camera.Right * farWidth;
	glm::vec3 farBottomLeft = farCenter - camera.Up * farHeight - camera.Right * farWidth;
	glm::vec3 farBottomRight = farCenter - camera.Up * farHeight + camera.Right * farWidth;

	return
	{ glm::vec4(nearBottomLeft, 1.0f), glm::vec4(nearBottomRight, 1.0f), glm::vec4(nearTopRight, 1.0f), glm::vec4(nearTopLeft, 1.0f), glm::vec4(farBottomLeft, 1.0f), glm::vec4(farBottomRight, 1.0f), glm::vec4(farTopRight, 1.0f), glm::vec4(farTopLeft, 1.0f) };
}


glm::mat4 Renderer::calculateCascadeLightMatrix(const glm::vec3& lightDirection, const Camera& camera, float aspectRatio, float nearPlane, float farPlane) const
{
	auto corners = getFrustumCornersWorldSpace(camera, aspectRatio, nearPlane, farPlane);

	glm::vec3 center(0.0f);

	for (const auto& corner : corners)
	{
		center += glm::vec3(corner);
	}

	center /= static_cast<float>(corners.size());

	glm::vec3 up(0.0f, 1.0f, 0.0f);

	if (std::abs(glm::dot(lightDirection, up)) > 0.99f)
		up = glm::vec3(0.0f, 0.0f, 1.0f);

	float distance = 100.0f;

	glm::vec3 lightPos = center - lightDirection * distance;

	glm::mat4 lightView = glm::lookAt(lightPos, center, up);

	// A bounding sphere keeps the XY scale fixed when the camera rotates.
	const float halfDepth = (farPlane - nearPlane) * 0.5f;
	const float farHeight = farPlane * std::tan(glm::radians(camera.Zoom * 0.5f));
	const float farWidth = farHeight * aspectRatio;
	float radius = std::sqrt(farWidth * farWidth + farHeight * farHeight + halfDepth * halfDepth);
	// Leave room for the existing half-texel snapping and the 3x3 PCF kernel.
	radius *= static_cast<float>(DIRECTIONAL_SHADOW_SIZE) / static_cast<float>(DIRECTIONAL_SHADOW_SIZE - 4);

	float minZ = FLT_MAX;
	float maxZ = -FLT_MAX;

	for (const auto& corner : corners)
	{
		glm::vec4 lightSpace = lightView * corner;

		minZ = std::min(minZ, lightSpace.z);
		maxZ = std::max(maxZ, lightSpace.z);
	}

	float zPadding = 50.0f;

	minZ -= zPadding;
	maxZ += zPadding;

	glm::mat4 lightProjection = glm::ortho(-radius, radius, -radius, radius, -maxZ, -minZ);

	return lightProjection * lightView;
}


void Renderer::setTAAEnabled(bool enabled)
{
	if (taaEnabled == enabled)
		return;

	taaEnabled = enabled;
	taaFrameIndex = 0;
	taaHistoryReadIndex = 0;
	taaHistoryValid = false;
	hasPreviousTAAFrame = false;
	previousModelMatrices.clear();
}


void Renderer::destroyBloomFramebuffer()
{
	for (const auto& mip : bloomMips)
	{
		glDeleteTextures(1, &mip.texture);
	}

	bloomMips.clear();

	glDeleteFramebuffers(1, &bloomFBO);
	bloomFBO = 0;
	bloomReady = false;
}


unsigned int Renderer::getPostTAAColorTexture(int debugView) const
{
	return taaEnabled && taaHistoryValid && (debugView == 0 || debugView == 10) ? taaHistoryColor[taaHistoryReadIndex] : sceneColorTexture;
}
