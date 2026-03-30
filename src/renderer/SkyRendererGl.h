// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct GlVec3
{
	float x, y, z;
};

struct GlAtmosphereInfo
{
	GlVec3 rayleigh_scattering = {};
	GlVec3 mie_scattering = {};
	GlVec3 mie_extinction = {};
	GlVec3 mie_absorption = {};
	GlVec3 absorption_extinction = {};
	GlVec3 ground_albedo = {};
	float bottom_radius = 0.0f;
	float top_radius = 0.0f;
	float mie_phase_g = 0.0f;
	float rayleigh_density_exp_scale = 0.0f;
	float mie_density_exp_scale = 0.0f;
	float absorption_layer0_width = 0.0f;
	float absorption_layer0_linear_term = 0.0f;
	float absorption_layer0_constant_term = 0.0f;
	float absorption_layer1_linear_term = 0.0f;
	float absorption_layer1_constant_term = 0.0f;
};

struct GlLutInfo
{
	unsigned int TRANSMITTANCE_TEXTURE_WIDTH = 256;
	unsigned int TRANSMITTANCE_TEXTURE_HEIGHT = 64;
	unsigned int MULTI_SCATTERING_TEXTURE_SIZE = 32;
	unsigned int SKY_VIEW_TEXTURE_WIDTH = 192;
	unsigned int SKY_VIEW_TEXTURE_HEIGHT = 108;
	unsigned int AERIAL_PERSPECTIVE_TEXTURE_WIDTH = 32;
	unsigned int AERIAL_PERSPECTIVE_TEXTURE_HEIGHT = 32;
	unsigned int AERIAL_PERSPECTIVE_TEXTURE_DEPTH = 32;
};

class SkyRendererGl
{
public:
	SkyRendererGl() = default;
	~SkyRendererGl() = default;

	bool initialise();
	void shutdown();
	void resize(int width, int height);
	void render();

	void setExternalSceneTextures(unsigned int hdrTexture, unsigned int linearDepthTexture);
	void clearExternalSceneTextures();
	void setExternalShadowMapTexture(unsigned int depthCompareTexture);
	void clearExternalShadowMapTexture();
	void setExternalShadowViewProj(const float* matrix4x4ColumnMajor);

	void setAerialPerspectiveDebugDepthKm(float depthKm) { mAerialPerspectiveDebugDepthKm = depthKm; }
	void setCameraHeight(float value);
	void setCameraForward(float value);
	void setCameraOffset(const GlVec3& value);
	void setViewYaw(float value);
	void setViewPitch(float value);
	void setSunIlluminanceScale(float value);
	void setSunYaw(float value);
	void setSunPitch(float value);
	void setRayMarchMinSpp(int value);
	void setRayMarchMaxSpp(int value);
	void setFastSky(bool enabled);
	void setFastAerialPerspective(bool enabled);
	void setShadowMapsEnabled(bool enabled)
	{
		if (mShadowMapsEnabled != enabled)
		{
			mShadowMapsEnabled = enabled;
			markSkyAndApDirty();
		}
	}
	void setColoredTransmittance(bool enabled) { mColoredTransmittance = enabled; }
	void setAerialPerspectivePreviewSlice(int value);
	void setMultiScatteringPreviewExposure(float value) { mMultiScatteringPreviewExposure = value; }
	void setAerialPerspectivePreviewExposure(float value) { mAerialPerspectivePreviewExposure = value; }
	void setUseAgxTonemap(bool enabled) { mUseAgxTonemap = enabled; }
	void setAutoExposureEnabled(bool enabled) { mAutoExposureEnabled = enabled; }
	void setManualExposure(float value);
	void setExposureBiasEv(float value);
	void setUseHistogramAutoExposure(bool enabled) { mUseHistogramAutoExposure = enabled; }
	void setAutoExposureHistogramLowPercent(float value);
	void setAutoExposureHistogramHighPercent(float value);
	void setSunAngleExposureBiasEnabled(bool enabled) { mSunAngleExposureBiasEnabled = enabled; }
	void setSunAngleExposureBiasAtHorizonEv(float value);
	void setSunAngleExposureBiasAtNoonEv(float value);
	void setDisplayGamma(float value);
	void setAutoExposureKey(float value);
	void setAgxSaturation(float value);
	void setPhysicalModeEnabled(bool enabled) { mPhysicalModeEnabled = enabled; }
	void setCameraEv100(float value);
	void setOutputSrgb(bool enabled) { mOutputSrgb = enabled; }
	void setMultipleScatteringFactor(float value);
	void setAtmosphereInfo(const GlAtmosphereInfo& value);

	float getCameraHeight() const { return mCameraHeight; }
	float getCameraForward() const { return mCameraForward; }
	GlVec3 getCameraOffset() const { return mCameraOffset; }
	float getViewYaw() const { return mViewYaw; }
	float getViewPitch() const { return mViewPitch; }
	GlVec3 getViewDir() const { return mViewDir; }
	GlVec3 getViewRight() const { return mViewRight; }
	GlVec3 getViewUp() const { return mViewUp; }
	float getSunIlluminanceScale() const { return mSunIlluminanceScale; }
	float getSunYaw() const { return mSunYaw; }
	float getSunPitch() const { return mSunPitch; }
	int getRayMarchMinSpp() const { return mRayMarchMinSpp; }
	int getRayMarchMaxSpp() const { return mRayMarchMaxSpp; }
	bool getFastSky() const { return mFastSky; }
	bool getFastAerialPerspective() const { return mFastAerialPerspective; }
	bool getShadowMapsEnabled() const { return mShadowMapsEnabled; }
	bool getColoredTransmittance() const { return mColoredTransmittance; }
	float getMultipleScatteringFactor() const { return mMultipleScatteringFactor; }
	bool getUseAgxTonemap() const { return mUseAgxTonemap; }
	bool getAutoExposureEnabled() const { return mAutoExposureEnabled; }
	float getManualExposure() const { return mManualExposure; }
	float getExposureBiasEv() const { return mExposureBiasEv; }
	bool getUseHistogramAutoExposure() const { return mUseHistogramAutoExposure; }
	float getAutoExposureHistogramLowPercent() const { return mAutoExposureHistogramLowPercent; }
	float getAutoExposureHistogramHighPercent() const { return mAutoExposureHistogramHighPercent; }
	bool getSunAngleExposureBiasEnabled() const { return mSunAngleExposureBiasEnabled; }
	float getSunAngleExposureBiasAtHorizonEv() const { return mSunAngleExposureBiasAtHorizonEv; }
	float getSunAngleExposureBiasAtNoonEv() const { return mSunAngleExposureBiasAtNoonEv; }
	float getDisplayGamma() const { return mDisplayGamma; }
	float getAutoExposureKey() const { return mAutoExposureKey; }
	float getAgxSaturation() const { return mAgxSaturation; }
	bool getPhysicalModeEnabled() const { return mPhysicalModeEnabled; }
	float getCameraEv100() const { return mCameraEv100; }
	bool getOutputSrgb() const { return mOutputSrgb; }
	GlAtmosphereInfo getAtmosphereInfo() const { return mAtmosphereInfo; }

	unsigned int getTransmittanceTexture() const { return mTransmittanceTex; }
	unsigned int getMultipleScatteringPreviewTexture() const { return mMultiScatteringPreviewTex; }
	unsigned int getSkyViewTexture() const { return mSkyViewTex; }
	unsigned int getAerialPerspectivePreviewTexture() const { return mAerialPerspectivePreviewTex; }
	int getAerialPerspectiveDepthSliceCount() const { return static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH); }
	int getAerialPerspectivePreviewSlice() const { return mAerialPerspectivePreviewSlice; }
	float getMultiScatteringDebugMin() const { return mMultiScatteringDebugMin; }
	float getMultiScatteringDebugMax() const { return mMultiScatteringDebugMax; }
	bool hasMultiScatteringDebugStats() const { return mMultiScatteringStatsValid; }
	float getAerialPerspectiveDebugMin() const { return mAerialPerspectiveDebugMin; }
	float getAerialPerspectiveDebugMax() const { return mAerialPerspectiveDebugMax; }
	bool hasAerialPerspectiveDebugStats() const { return mAerialPerspectiveStatsValid; }
	bool isInitialised() const { return mInitialised; }
	bool hasGpuPassTimings() const { return mGpuPassTimingsSupported; }
	float getTransmittancePassMs() const { return mTransmittancePassTimer.lastMs; }
	float getMultiScatteringPassMs() const { return mMultiScatteringPassTimer.lastMs; }
	float getSkyViewPassMs() const { return mSkyViewPassTimer.lastMs; }
	float getAerialPerspectivePassMs() const { return mAerialPerspectivePassTimer.lastMs; }
	float getPresentPassMs() const { return mPresentPassTimer.lastMs; }

private:
	struct GpuPassTimer
	{
		unsigned int query = 0;
		float lastMs = 0.0f;
		bool pending = false;
		bool active = false;
	};

	bool createPrograms();
	void createGpuPassTimers();
	bool createAutoExposureResources();
	bool createTransmittanceResources();
	bool createMultipleScatteringResources();
	bool createSkyViewResources();
	bool createAerialPerspectiveResources();
	bool createShadowResources();
	bool createSceneResources();
	void destroyPrograms();
	void destroyGpuPassTimers();
	void destroyAutoExposureResources();
	void destroyTransmittanceResources();
	void destroyMultipleScatteringResources();
	void destroySkyViewResources();
	void destroyAerialPerspectiveResources();
	void destroyShadowResources();
	void destroySceneResources();
	void renderTransmittanceLut();
	void renderMultipleScatteringLut();
	void renderSkyViewLut();
	void renderAerialPerspectiveVolume();
	void renderPresent();
	void runAutoExposureHistogram();
	void copyAerialPerspectivePreviewSlice();
	void updateLutPreviewTextures();
	void uploadAtmosphereUniforms(unsigned int program);
	void updateMultiScatteringDebugStats();
	void updateAerialPerspectiveDebugStats();
	void updateViewAndSunDirections();
	void resolveGpuPassTimers();
	void beginGpuPassTimer(GpuPassTimer& timer);
	void endGpuPassTimer(GpuPassTimer& timer);
	void markLutsDirty();
	void markSkyAndApDirty();

	unsigned int loadAndCompileShader(unsigned int type, const char* path);
	unsigned int linkProgram(unsigned int vs, unsigned int fs, const char* debugName);
	unsigned int linkComputeProgram(unsigned int cs, const char* debugName);

	bool mInitialised = false;
	bool mGpuPassTimingsSupported = false;
	bool mLutDirty = true;
	bool mSkyViewDirty = true;
	bool mAerialPerspectiveDirty = true;
	int mBackbufferWidth = 1280;
	int mBackbufferHeight = 720;

	GlLutInfo mLutsInfo;
	GlAtmosphereInfo mAtmosphereInfo = {};
	float mMultipleScatteringFactor = 1.0f;
	float mCameraHeight = 0.5f;
	float mCameraForward = -1.0f;
	float mViewYaw = 0.0f;
	float mViewPitch = 0.0f;
	float mSunIlluminanceScale = 1.0f;
	float mSunYaw = 0.0f;
	float mSunPitch = 0.45f;
	int mRayMarchMinSpp = 4;
	int mRayMarchMaxSpp = 14;
	bool mFastSky = true;
	bool mFastAerialPerspective = true;
	bool mShadowMapsEnabled = true;
	bool mColoredTransmittance = false;
	GlVec3 mViewDir = { 0.0f, 1.0f, 0.0f };
	GlVec3 mViewRight = { 1.0f, 0.0f, 0.0f };
	GlVec3 mViewUp = { 0.0f, 0.0f, 1.0f };
	GlVec3 mCameraPosition = { 0.0f, -1.0f, 0.5f };
	GlVec3 mCameraOffset = { 0.0f, -1.0f, 0.5f };
	GlVec3 mSunDir = { 0.0f, 0.70710678f, 0.70710678f };

	unsigned int mFullscreenVao = 0;
	unsigned int mTransmittanceProgram = 0;
	unsigned int mMultiScatteringProgram = 0;
	unsigned int mSkyViewProgram = 0;
	unsigned int mAerialPerspectiveProgram = 0;
	unsigned int mRaymarchProgram = 0;
	unsigned int mPostProcessProgram = 0;
	unsigned int mAutoExposureHistogramProgram = 0;
	unsigned int mAutoExposureReduceProgram = 0;

	unsigned int mTransmittanceTex = 0;
	unsigned int mTransmittanceFbo = 0;
	unsigned int mMultiScatteringTex = 0;
	unsigned int mMultiScatteringPreviewTex = 0;
	unsigned int mSkyViewTex = 0;
	unsigned int mSkyViewFbo = 0;
	unsigned int mAerialPerspectiveTex = 0;
	unsigned int mAerialPerspectivePreviewTex = 0;
	unsigned int mShadowFallbackTex = 0;
	unsigned int mSceneFbo = 0;
	unsigned int mSceneHdrTex = 0;
	unsigned int mSceneLinearDepthTex = 0;
	unsigned int mSceneDepthTex = 0;
	unsigned int mFinalHdrFbo = 0;
	unsigned int mFinalHdrTex = 0;
	unsigned int mAutoExposureHistogramSsbo = 0;
	unsigned int mAutoExposureMeterTex = 0;
	float mMultiScatteringDebugMin = 0.0f;
	float mMultiScatteringDebugMax = 0.0f;
	bool mMultiScatteringStatsValid = false;
	float mAerialPerspectiveDebugMin = 0.0f;
	float mAerialPerspectiveDebugMax = 0.0f;
	bool mAerialPerspectiveStatsValid = false;
	int mAerialPerspectivePreviewSlice = 0;
	float mMultiScatteringPreviewExposure = 32.0f;
	float mAerialPerspectivePreviewExposure = 16.0f;
	float mAerialPerspectiveDebugDepthKm = 16.0f;
	bool mUseAgxTonemap = true;
	bool mAutoExposureEnabled = true;
	float mManualExposure = 1.0f;
	float mExposureBiasEv = 0.0f;
	bool mUseHistogramAutoExposure = true;
	float mAutoExposureHistogramLowPercent = 50.0f;
	float mAutoExposureHistogramHighPercent = 98.0f;
	bool mSunAngleExposureBiasEnabled = true;
	float mSunAngleExposureBiasAtHorizonEv = -0.7f;
	float mSunAngleExposureBiasAtNoonEv = 0.7f;
	float mDisplayGamma = 1.0f;
	float mAutoExposureKey = 0.10f;
	float mAgxSaturation = 1.15f;
	bool mPhysicalModeEnabled = false;
	float mCameraEv100 = 15.0f;
	bool mOutputSrgb = true;
	int mFinalHdrMipLevel = 0;
	float mShadowViewProj[16] = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	bool mHasExternalSceneTextures = false;
	unsigned int mExternalSceneHdrTex = 0;
	unsigned int mExternalSceneLinearDepthTex = 0;
	bool mHasExternalShadowMapTexture = false;
	unsigned int mExternalShadowMapDepthTex = 0;
	GpuPassTimer mTransmittancePassTimer = {};
	GpuPassTimer mMultiScatteringPassTimer = {};
	GpuPassTimer mSkyViewPassTimer = {};
	GpuPassTimer mAerialPerspectivePassTimer = {};
	GpuPassTimer mPresentPassTimer = {};
};
