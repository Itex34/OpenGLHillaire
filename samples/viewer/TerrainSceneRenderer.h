#pragma once

#include "PbrSkyLibOpenGL/SkyAtmosphereRenderer.h"

class TerrainSceneRenderer
{
public:
	bool initialise();
	void shutdown();
	void resize(int width, int height);
	void render(const pbrsky::SkyAtmosphereRenderer& renderer, bool renderTerrain, bool shadowMapsEnabled);

	unsigned int getSceneColorTexture() const { return mSceneHdrTex; }
	unsigned int getSceneLinearDepthTexture() const { return mSceneLinearDepthTex; }
	unsigned int getShadowDepthTexture() const { return mShadowDepthTex; }
	const float* getShadowViewProjMatrix() const { return mShadowViewProj; }
	float getTerrainPassMs() const { return mTerrainPassMs; }

private:
	bool createPrograms();
	bool createHeightmap();
	bool createShadowResources();
	bool createSceneResources();
	void destroyPrograms();
	void destroyHeightmap();
	void destroyShadowResources();
	void destroySceneResources();

	void updateShadowViewProj(const pbrsky::Vec3& sunDir);
	void resolvePassTimer();
	void beginPassTimer();
	void endPassTimer();

	unsigned int loadAndCompileShader(unsigned int type, const char* path);
	unsigned int linkProgram(unsigned int vs, unsigned int fs, const char* debugName);

	int mBackbufferWidth = 1;
	int mBackbufferHeight = 1;

	unsigned int mDummyVao = 0;
	unsigned int mTerrainProgram = 0;
	unsigned int mTerrainShadowProgram = 0;
	unsigned int mHeightmapTex = 0;

	unsigned int mShadowFbo = 0;
	unsigned int mShadowDepthTex = 0;
	unsigned int mShadowFallbackTex = 0;
	unsigned int mShadowMapSize = 4096;

	unsigned int mSceneFbo = 0;
	unsigned int mSceneHdrTex = 0;
	unsigned int mSceneLinearDepthTex = 0;
	unsigned int mSceneDepthTex = 0;

	unsigned int mPassTimerQuery = 0;
	bool mPassTimerPending = false;
	float mTerrainPassMs = 0.0f;

	float mShadowViewProj[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
};
