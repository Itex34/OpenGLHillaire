#include "TerrainSceneRenderer.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <GL/gl3w.h>
#include <tinyexr/tinyexr.h>

#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	constexpr const char* kTerrainVertexShaderPath = "Resources/glsl/terrain.vert";
	constexpr const char* kTerrainFragmentShaderPath = "Resources/glsl/terrain.frag";
	constexpr const char* kTerrainShadowFragmentShaderPath = "Resources/glsl/terrain_shadow.frag";
	constexpr float kMainCameraFovYDegrees = 66.6f;

	std::string readTextFile(const char* path)
	{
		const std::string originalPath(path);
		const std::string candidates[] = {
			originalPath,
			"../" + originalPath,
			"../../" + originalPath,
			"../../../" + originalPath,
			"../../../../" + originalPath,
		};

		for (const std::string& candidate : candidates)
		{
			std::ifstream file(candidate.c_str(), std::ios::in | std::ios::binary);
			if (!file.is_open())
			{
				continue;
			}

			std::ostringstream ss;
			ss << file.rdbuf();
			return ss.str();
		}

		return std::string();
	}

	void showMessageBox(const char* title, const std::string& message)
	{
		std::fprintf(stderr, "%s\n%s\n", title, message.c_str());
#if defined(_WIN32)
		MessageBoxA(nullptr, message.c_str(), title, MB_ICONERROR | MB_OK);
#endif
	}

	bool loadExrRgba(const char* path, int& outWidth, int& outHeight, std::vector<float>& outPixels, std::string& outError)
	{
		const std::string originalPath(path);
		const std::string candidates[] = {
			originalPath,
			"../" + originalPath,
			"../../" + originalPath,
			"../../../" + originalPath,
			"../../../../" + originalPath,
		};

		for (const std::string& candidate : candidates)
		{
			float* rgba = nullptr;
			int width = 0;
			int height = 0;
			const char* exrError = nullptr;
			const int result = LoadEXR(&rgba, &width, &height, candidate.c_str(), &exrError);
			if (result == TINYEXR_SUCCESS && rgba != nullptr && width > 0 && height > 0)
			{
				outWidth = width;
				outHeight = height;
				const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
				outPixels.assign(rgba, rgba + pixelCount);
				std::free(rgba);
				return true;
			}
			if (exrError != nullptr)
			{
				outError = exrError;
				FreeEXRErrorMessage(exrError);
			}
		}

		if (outError.empty())
		{
			outError = "Heightmap EXR not found.";
		}
		return false;
	}

	glm::vec3 makeViewDirectionZUpForwardY(float yaw, float pitch)
	{
		const float cp = std::cos(pitch);
		const glm::vec3 dir(std::sin(yaw) * cp, std::cos(yaw) * cp, std::sin(pitch));
		const float len = glm::length(dir);
		return (len > 1e-6f) ? (dir / len) : glm::vec3(0.0f, 1.0f, 0.0f);
	}
}

unsigned int TerrainSceneRenderer::loadAndCompileShader(unsigned int type, const char* path)
{
	const std::string source = readTextFile(path);
	if (source.empty())
	{
		showMessageBox("OpenGL shader error", std::string("Failed to read shader file: ") + path);
		return 0;
	}

	const char* src = source.c_str();
	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, nullptr);
	glCompileShader(shader);

	int success = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (success == GL_TRUE)
	{
		return shader;
	}

	int logLength = 0;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
	std::string log;
	log.resize(logLength > 1 ? static_cast<size_t>(logLength) : 1u);
	glGetShaderInfoLog(shader, logLength, nullptr, &log[0]);
	showMessageBox("OpenGL shader compile error", std::string("Shader: ") + path + "\n\n" + log);
	glDeleteShader(shader);
	return 0;
}

unsigned int TerrainSceneRenderer::linkProgram(unsigned int vs, unsigned int fs, const char* debugName)
{
	unsigned int program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	int success = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if (success == GL_TRUE)
	{
		return program;
	}

	int logLength = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
	std::string log;
	log.resize(logLength > 1 ? static_cast<size_t>(logLength) : 1u);
	glGetProgramInfoLog(program, logLength, nullptr, &log[0]);
	showMessageBox("OpenGL program link error", std::string("Program: ") + debugName + "\n\n" + log);
	glDeleteProgram(program);
	return 0;
}

bool TerrainSceneRenderer::createPrograms()
{
	const unsigned int terrainVs = loadAndCompileShader(GL_VERTEX_SHADER, kTerrainVertexShaderPath);
	const unsigned int terrainFs = loadAndCompileShader(GL_FRAGMENT_SHADER, kTerrainFragmentShaderPath);
	const unsigned int terrainShadowFs = loadAndCompileShader(GL_FRAGMENT_SHADER, kTerrainShadowFragmentShaderPath);
	if (terrainVs == 0 || terrainFs == 0 || terrainShadowFs == 0)
	{
		if (terrainVs != 0) glDeleteShader(terrainVs);
		if (terrainFs != 0) glDeleteShader(terrainFs);
		if (terrainShadowFs != 0) glDeleteShader(terrainShadowFs);
		return false;
	}

	mTerrainProgram = linkProgram(terrainVs, terrainFs, "Terrain");
	mTerrainShadowProgram = linkProgram(terrainVs, terrainShadowFs, "TerrainShadow");
	glDeleteShader(terrainVs);
	glDeleteShader(terrainFs);
	glDeleteShader(terrainShadowFs);

	if (mTerrainProgram == 0 || mTerrainShadowProgram == 0)
	{
		destroyPrograms();
		return false;
	}
	return true;
}

bool TerrainSceneRenderer::createHeightmap()
{
	int width = 0;
	int height = 0;
	std::vector<float> pixels;
	std::string exrError;
	const bool loaded = loadExrRgba("Resources/heightmap1.exr", width, height, pixels, exrError);

	glGenTextures(1, &mHeightmapTex);
	glBindTexture(GL_TEXTURE_2D, mHeightmapTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	if (loaded)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, pixels.data());
	}
	else
	{
		const float fallbackPixel[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, fallbackPixel);
		std::fprintf(stderr, "OpenGL terrain warning: failed to load heightmap1.exr. Using flat fallback terrain. %s\n", exrError.c_str());
	}
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

bool TerrainSceneRenderer::createShadowResources()
{
	const GLsizei shadowSize = static_cast<GLsizei>(mShadowMapSize);

	glGenTextures(1, &mShadowDepthTex);
	glBindTexture(GL_TEXTURE_2D, mShadowDepthTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &mShadowFallbackTex);
	glBindTexture(GL_TEXTURE_2D, mShadowFallbackTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	const float litDepth = 1.0f;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &litDepth);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &mShadowFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mShadowFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mShadowDepthTex, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	const unsigned int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		showMessageBox("OpenGL framebuffer error", "Failed to create terrain shadow framebuffer.");
		destroyShadowResources();
		return false;
	}
	return true;
}

bool TerrainSceneRenderer::createSceneResources()
{
	const GLsizei width = static_cast<GLsizei>(mBackbufferWidth);
	const GLsizei height = static_cast<GLsizei>(mBackbufferHeight);

	glGenTextures(1, &mSceneHdrTex);
	glBindTexture(GL_TEXTURE_2D, mSceneHdrTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &mSceneLinearDepthTex);
	glBindTexture(GL_TEXTURE_2D, mSceneLinearDepthTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &mSceneDepthTex);
	glBindTexture(GL_TEXTURE_2D, mSceneDepthTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &mSceneFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mSceneFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSceneHdrTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mSceneLinearDepthTex, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, mSceneDepthTex, 0);
	const unsigned int drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, drawBuffers);
	const unsigned int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		showMessageBox("OpenGL framebuffer error", "Failed to create terrain scene framebuffer.");
		destroySceneResources();
		return false;
	}
	return true;
}

void TerrainSceneRenderer::destroyPrograms()
{
	if (mTerrainShadowProgram != 0)
	{
		glDeleteProgram(mTerrainShadowProgram);
		mTerrainShadowProgram = 0;
	}
	if (mTerrainProgram != 0)
	{
		glDeleteProgram(mTerrainProgram);
		mTerrainProgram = 0;
	}
}

void TerrainSceneRenderer::destroyHeightmap()
{
	if (mHeightmapTex != 0)
	{
		glDeleteTextures(1, &mHeightmapTex);
		mHeightmapTex = 0;
	}
}

void TerrainSceneRenderer::destroyShadowResources()
{
	if (mShadowFbo != 0)
	{
		glDeleteFramebuffers(1, &mShadowFbo);
		mShadowFbo = 0;
	}
	if (mShadowDepthTex != 0)
	{
		glDeleteTextures(1, &mShadowDepthTex);
		mShadowDepthTex = 0;
	}
	if (mShadowFallbackTex != 0)
	{
		glDeleteTextures(1, &mShadowFallbackTex);
		mShadowFallbackTex = 0;
	}
}

void TerrainSceneRenderer::destroySceneResources()
{
	if (mSceneFbo != 0)
	{
		glDeleteFramebuffers(1, &mSceneFbo);
		mSceneFbo = 0;
	}
	if (mSceneDepthTex != 0)
	{
		glDeleteTextures(1, &mSceneDepthTex);
		mSceneDepthTex = 0;
	}
	if (mSceneLinearDepthTex != 0)
	{
		glDeleteTextures(1, &mSceneLinearDepthTex);
		mSceneLinearDepthTex = 0;
	}
	if (mSceneHdrTex != 0)
	{
		glDeleteTextures(1, &mSceneHdrTex);
		mSceneHdrTex = 0;
	}
}

bool TerrainSceneRenderer::initialise()
{
	glGenVertexArrays(1, &mDummyVao);
	glGenQueries(1, &mPassTimerQuery);
	if (!createPrograms() || !createHeightmap() || !createShadowResources() || !createSceneResources())
	{
		shutdown();
		return false;
	}
	return true;
}

void TerrainSceneRenderer::shutdown()
{
	destroySceneResources();
	destroyShadowResources();
	destroyHeightmap();
	destroyPrograms();
	if (mPassTimerQuery != 0)
	{
		glDeleteQueries(1, &mPassTimerQuery);
		mPassTimerQuery = 0;
	}
	mPassTimerPending = false;
	mTerrainPassMs = 0.0f;
	if (mDummyVao != 0)
	{
		glDeleteVertexArrays(1, &mDummyVao);
		mDummyVao = 0;
	}
}

void TerrainSceneRenderer::resize(int width, int height)
{
	const int newWidth = width > 1 ? width : 1;
	const int newHeight = height > 1 ? height : 1;
	if (newWidth == mBackbufferWidth && newHeight == mBackbufferHeight)
	{
		return;
	}
	mBackbufferWidth = newWidth;
	mBackbufferHeight = newHeight;
	destroySceneResources();
	createSceneResources();
}

void TerrainSceneRenderer::updateShadowViewProj(const pbrsky::Vec3& sunDir)
{
	const glm::vec3 eyePosition(0.0f, 0.0f, 0.0f);
	const glm::vec3 focusPosition(-sunDir.x, -sunDir.y, -sunDir.z);
	const glm::vec3 upDirection(0.0f, 0.0f, 1.0f);
	const glm::mat4 viewMatrix = glm::lookAtLH(eyePosition, focusPosition, upDirection);
	const glm::mat4 projMatrix = glm::orthoLH_NO(-100.0f, 100.0f, -100.0f, 100.0f, -100.0f, 100.0f);
	const glm::mat4 viewProjMatrix = projMatrix * viewMatrix;
	std::memcpy(mShadowViewProj, glm::value_ptr(viewProjMatrix), sizeof(mShadowViewProj));
}

void TerrainSceneRenderer::resolvePassTimer()
{
	if (mPassTimerQuery == 0 || !mPassTimerPending)
	{
		return;
	}
	unsigned int available = 0;
	glGetQueryObjectuiv(mPassTimerQuery, GL_QUERY_RESULT_AVAILABLE, &available);
	if (available == 0)
	{
		return;
	}
	GLuint64 elapsedNs = 0;
	glGetQueryObjectui64v(mPassTimerQuery, GL_QUERY_RESULT, &elapsedNs);
	mTerrainPassMs = static_cast<float>(elapsedNs * (1.0 / 1000000.0));
	mPassTimerPending = false;
}

void TerrainSceneRenderer::beginPassTimer()
{
	if (mPassTimerQuery == 0 || mPassTimerPending)
	{
		return;
	}
	glBeginQuery(GL_TIME_ELAPSED, mPassTimerQuery);
}

void TerrainSceneRenderer::endPassTimer()
{
	if (mPassTimerQuery == 0 || mPassTimerPending)
	{
		return;
	}
	glEndQuery(GL_TIME_ELAPSED);
	mPassTimerPending = true;
}

void TerrainSceneRenderer::render(const pbrsky::SkyAtmosphereRenderer& renderer, bool renderTerrain, bool shadowMapsEnabled)
{
	resolvePassTimer();

	const pbrsky::Vec3 sunDir = makeViewDirectionZUpForwardY(renderer.getSunYaw(), renderer.getSunPitch());
	updateShadowViewProj(sunDir);

	beginPassTimer();

	if (shadowMapsEnabled && renderTerrain)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mShadowFbo);
		glViewport(0, 0, static_cast<GLsizei>(mShadowMapSize), static_cast<GLsizei>(mShadowMapSize));
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
		glDisable(GL_CULL_FACE);
		const float clearDepth = 1.0f;
		glClearBufferfv(GL_DEPTH, 0, &clearDepth);

		glUseProgram(mTerrainShadowProgram);
		const int terrainResLoc = glGetUniformLocation(mTerrainShadowProgram, "u_terrain_resolution");
		if (terrainResLoc >= 0) glUniform1i(terrainResLoc, 512);
		const int viewProjLoc = glGetUniformLocation(mTerrainShadowProgram, "u_view_proj");
		if (viewProjLoc >= 0) glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, mShadowViewProj);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mHeightmapTex);
		const int heightmapLoc = glGetUniformLocation(mTerrainShadowProgram, "u_heightmap_tex");
		if (heightmapLoc >= 0) glUniform1i(heightmapLoc, 0);

		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(5.0f, 5.0f);
		glBindVertexArray(mDummyVao);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 512 * 512);
		glBindVertexArray(0);
		glDisable(GL_POLYGON_OFFSET_FILL);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, mSceneFbo);
	glViewport(0, 0, mBackbufferWidth, mBackbufferHeight);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float clearLinearDepth[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	const float clearDepth = 1.0f;
	glClearBufferfv(GL_COLOR, 0, clearColor);
	glClearBufferfv(GL_COLOR, 1, clearLinearDepth);
	glClearBufferfv(GL_DEPTH, 0, &clearDepth);

	if (renderTerrain)
	{
		const pbrsky::Vec3 cameraWorld = renderer.getCameraOffset();
		const pbrsky::Vec3 viewDir = renderer.getViewDir();
		const glm::vec3 eyePosition(cameraWorld.x, cameraWorld.y, cameraWorld.z);
		const glm::vec3 focusPosition(cameraWorld.x + viewDir.x, cameraWorld.y + viewDir.y, cameraWorld.z + viewDir.z);
		const glm::vec3 upDirection(0.0f, 0.0f, 1.0f);
		const glm::mat4 viewMatrix = glm::lookAtLH(eyePosition, focusPosition, upDirection);
		const float aspectRatio = static_cast<float>(mBackbufferWidth) / static_cast<float>(mBackbufferHeight);
		const glm::mat4 projMatrix = glm::perspectiveLH_NO(glm::radians(kMainCameraFovYDegrees), aspectRatio, 0.1f, 20000.0f);
		const glm::mat4 viewProjMatrix = projMatrix * viewMatrix;
		const pbrsky::AtmosphereInfo atmosphere = renderer.getAtmosphereInfo();

		glUseProgram(mTerrainProgram);
		const int terrainResLoc = glGetUniformLocation(mTerrainProgram, "u_terrain_resolution");
		if (terrainResLoc >= 0) glUniform1i(terrainResLoc, 512);
		const int viewProjLoc = glGetUniformLocation(mTerrainProgram, "u_view_proj");
		if (viewProjLoc >= 0) glUniformMatrix4fv(viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProjMatrix));
		const int shadowViewProjLoc = glGetUniformLocation(mTerrainProgram, "u_shadow_view_proj");
		if (shadowViewProjLoc >= 0) glUniformMatrix4fv(shadowViewProjLoc, 1, GL_FALSE, mShadowViewProj);
		const int camPosLoc = glGetUniformLocation(mTerrainProgram, "u_camera_world_pos");
		if (camPosLoc >= 0) glUniform3f(camPosLoc, cameraWorld.x, cameraWorld.y, cameraWorld.z);
		const int sunDirLoc = glGetUniformLocation(mTerrainProgram, "u_sun_direction");
		if (sunDirLoc >= 0) glUniform3f(sunDirLoc, sunDir.x, sunDir.y, sunDir.z);
		const int bottomRadiusLoc = glGetUniformLocation(mTerrainProgram, "u_bottom_radius");
		if (bottomRadiusLoc >= 0) glUniform1f(bottomRadiusLoc, atmosphere.bottom_radius);
		const int topRadiusLoc = glGetUniformLocation(mTerrainProgram, "u_top_radius");
		if (topRadiusLoc >= 0) glUniform1f(topRadiusLoc, atmosphere.top_radius);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mHeightmapTex);
		const int heightmapLoc = glGetUniformLocation(mTerrainProgram, "u_heightmap_tex");
		if (heightmapLoc >= 0) glUniform1i(heightmapLoc, 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, renderer.getTransmittanceTexture());
		const int transLoc = glGetUniformLocation(mTerrainProgram, "u_transmittance_lut");
		if (transLoc >= 0) glUniform1i(transLoc, 1);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, shadowMapsEnabled ? mShadowDepthTex : mShadowFallbackTex);
		const int shadowTexLoc = glGetUniformLocation(mTerrainProgram, "u_shadowmap_tex");
		if (shadowTexLoc >= 0) glUniform1i(shadowTexLoc, 2);

		glBindVertexArray(mDummyVao);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 512 * 512);
		glBindVertexArray(0);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);
		glUseProgram(0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	endPassTimer();
}
