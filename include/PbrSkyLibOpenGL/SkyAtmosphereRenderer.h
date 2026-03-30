#pragma once

#include <memory>

namespace pbrsky
{

struct Vec3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

struct AtmosphereInfo
{
	Vec3 rayleigh_scattering = {};
	Vec3 mie_scattering = {};
	Vec3 mie_extinction = {};
	Vec3 mie_absorption = {};
	Vec3 absorption_extinction = {};
	Vec3 ground_albedo = {};
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

class SkyAtmosphereRenderer
{
public:
	SkyAtmosphereRenderer();
	~SkyAtmosphereRenderer();

	SkyAtmosphereRenderer(const SkyAtmosphereRenderer&) = delete;
	SkyAtmosphereRenderer& operator=(const SkyAtmosphereRenderer&) = delete;

	bool initialise();
	void shutdown();
	void resize(int width, int height);
	void render();

	void setExternalSceneTextures(unsigned int hdrTexture, unsigned int linearDepthTexture);
	void clearExternalSceneTextures();
	void setExternalShadowMapTexture(unsigned int depthCompareTexture);
	void clearExternalShadowMapTexture();
	void setExternalShadowViewProj(const float* matrix4x4ColumnMajor);

	void setAerialPerspectiveDebugDepthKm(float depthKm);
	void setCameraHeight(float value);
	void setCameraForward(float value);
	void setCameraOffset(const Vec3& value);
	void setViewYaw(float value);
	void setViewPitch(float value);
	void setSunIlluminanceScale(float value);
	void setSunYaw(float value);
	void setSunPitch(float value);
	void setRayMarchMinSpp(int value);
	void setRayMarchMaxSpp(int value);
	void setFastSky(bool enabled);
	void setFastAerialPerspective(bool enabled);
	void setShadowMapsEnabled(bool enabled);
	void setColoredTransmittance(bool enabled);
	void setAerialPerspectivePreviewSlice(int value);
	void setMultiScatteringPreviewExposure(float value);
	void setAerialPerspectivePreviewExposure(float value);
	void setUseAgxTonemap(bool enabled);
	void setAutoExposureEnabled(bool enabled);
	void setManualExposure(float value);
	void setExposureBiasEv(float value);
	void setUseHistogramAutoExposure(bool enabled);
	void setAutoExposureHistogramLowPercent(float value);
	void setAutoExposureHistogramHighPercent(float value);
	void setSunAngleExposureBiasEnabled(bool enabled);
	void setSunAngleExposureBiasAtHorizonEv(float value);
	void setSunAngleExposureBiasAtNoonEv(float value);
	void setDisplayGamma(float value);
	void setAutoExposureKey(float value);
	void setAgxSaturation(float value);
	void setPhysicalModeEnabled(bool enabled);
	void setCameraEv100(float value);
	void setOutputSrgb(bool enabled);
	void setMultipleScatteringFactor(float value);
	void setAtmosphereInfo(const AtmosphereInfo& value);

	float getCameraHeight() const;
	float getCameraForward() const;
	Vec3 getCameraOffset() const;
	float getViewYaw() const;
	float getViewPitch() const;
	Vec3 getViewDir() const;
	Vec3 getViewRight() const;
	Vec3 getViewUp() const;
	float getSunIlluminanceScale() const;
	float getSunYaw() const;
	float getSunPitch() const;
	int getRayMarchMinSpp() const;
	int getRayMarchMaxSpp() const;
	bool getFastSky() const;
	bool getFastAerialPerspective() const;
	bool getShadowMapsEnabled() const;
	bool getColoredTransmittance() const;
	float getMultipleScatteringFactor() const;
	bool getUseAgxTonemap() const;
	bool getAutoExposureEnabled() const;
	float getManualExposure() const;
	float getExposureBiasEv() const;
	bool getUseHistogramAutoExposure() const;
	float getAutoExposureHistogramLowPercent() const;
	float getAutoExposureHistogramHighPercent() const;
	bool getSunAngleExposureBiasEnabled() const;
	float getSunAngleExposureBiasAtHorizonEv() const;
	float getSunAngleExposureBiasAtNoonEv() const;
	float getDisplayGamma() const;
	float getAutoExposureKey() const;
	float getAgxSaturation() const;
	bool getPhysicalModeEnabled() const;
	float getCameraEv100() const;
	bool getOutputSrgb() const;
	AtmosphereInfo getAtmosphereInfo() const;

	unsigned int getTransmittanceTexture() const;
	unsigned int getMultipleScatteringPreviewTexture() const;
	unsigned int getSkyViewTexture() const;
	unsigned int getAerialPerspectivePreviewTexture() const;
	int getAerialPerspectiveDepthSliceCount() const;
	int getAerialPerspectivePreviewSlice() const;
	float getMultiScatteringDebugMin() const;
	float getMultiScatteringDebugMax() const;
	bool hasMultiScatteringDebugStats() const;
	float getAerialPerspectiveDebugMin() const;
	float getAerialPerspectiveDebugMax() const;
	bool hasAerialPerspectiveDebugStats() const;
	bool isInitialised() const;
	bool hasGpuPassTimings() const;
	float getTransmittancePassMs() const;
	float getMultiScatteringPassMs() const;
	float getSkyViewPassMs() const;
	float getAerialPerspectivePassMs() const;
	float getPresentPassMs() const;

private:
	struct Impl;
	std::unique_ptr<Impl> mImpl;
};

}
