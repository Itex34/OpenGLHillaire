#include "PbrSkyLibOpenGL/SkyAtmosphereRenderer.h"

#include "renderer/SkyRendererGl.h"

namespace pbrsky
{

namespace
{
	static GlVec3 toInternal(const Vec3& value)
	{
		return { value.x, value.y, value.z };
	}

	static Vec3 toPublic(const GlVec3& value)
	{
		return { value.x, value.y, value.z };
	}

	static GlAtmosphereInfo toInternal(const AtmosphereInfo& value)
	{
		GlAtmosphereInfo out;
		out.rayleigh_scattering = toInternal(value.rayleigh_scattering);
		out.mie_scattering = toInternal(value.mie_scattering);
		out.mie_extinction = toInternal(value.mie_extinction);
		out.mie_absorption = toInternal(value.mie_absorption);
		out.absorption_extinction = toInternal(value.absorption_extinction);
		out.ground_albedo = toInternal(value.ground_albedo);
		out.bottom_radius = value.bottom_radius;
		out.top_radius = value.top_radius;
		out.mie_phase_g = value.mie_phase_g;
		out.rayleigh_density_exp_scale = value.rayleigh_density_exp_scale;
		out.mie_density_exp_scale = value.mie_density_exp_scale;
		out.absorption_layer0_width = value.absorption_layer0_width;
		out.absorption_layer0_linear_term = value.absorption_layer0_linear_term;
		out.absorption_layer0_constant_term = value.absorption_layer0_constant_term;
		out.absorption_layer1_linear_term = value.absorption_layer1_linear_term;
		out.absorption_layer1_constant_term = value.absorption_layer1_constant_term;
		return out;
	}

	static AtmosphereInfo toPublic(const GlAtmosphereInfo& value)
	{
		AtmosphereInfo out;
		out.rayleigh_scattering = toPublic(value.rayleigh_scattering);
		out.mie_scattering = toPublic(value.mie_scattering);
		out.mie_extinction = toPublic(value.mie_extinction);
		out.mie_absorption = toPublic(value.mie_absorption);
		out.absorption_extinction = toPublic(value.absorption_extinction);
		out.ground_albedo = toPublic(value.ground_albedo);
		out.bottom_radius = value.bottom_radius;
		out.top_radius = value.top_radius;
		out.mie_phase_g = value.mie_phase_g;
		out.rayleigh_density_exp_scale = value.rayleigh_density_exp_scale;
		out.mie_density_exp_scale = value.mie_density_exp_scale;
		out.absorption_layer0_width = value.absorption_layer0_width;
		out.absorption_layer0_linear_term = value.absorption_layer0_linear_term;
		out.absorption_layer0_constant_term = value.absorption_layer0_constant_term;
		out.absorption_layer1_linear_term = value.absorption_layer1_linear_term;
		out.absorption_layer1_constant_term = value.absorption_layer1_constant_term;
		return out;
	}
}

struct SkyAtmosphereRenderer::Impl
{
	SkyRendererGl renderer;
};

SkyAtmosphereRenderer::SkyAtmosphereRenderer()
	: mImpl(std::make_unique<Impl>())
{
}

SkyAtmosphereRenderer::~SkyAtmosphereRenderer() = default;

bool SkyAtmosphereRenderer::initialise() { return mImpl->renderer.initialise(); }
void SkyAtmosphereRenderer::shutdown() { mImpl->renderer.shutdown(); }
void SkyAtmosphereRenderer::resize(int width, int height) { mImpl->renderer.resize(width, height); }
void SkyAtmosphereRenderer::render() { mImpl->renderer.render(); }

void SkyAtmosphereRenderer::setExternalSceneTextures(unsigned int hdrTexture, unsigned int linearDepthTexture) { mImpl->renderer.setExternalSceneTextures(hdrTexture, linearDepthTexture); }
void SkyAtmosphereRenderer::clearExternalSceneTextures() { mImpl->renderer.clearExternalSceneTextures(); }
void SkyAtmosphereRenderer::setExternalShadowMapTexture(unsigned int depthCompareTexture) { mImpl->renderer.setExternalShadowMapTexture(depthCompareTexture); }
void SkyAtmosphereRenderer::clearExternalShadowMapTexture() { mImpl->renderer.clearExternalShadowMapTexture(); }
void SkyAtmosphereRenderer::setExternalShadowViewProj(const float* matrix4x4ColumnMajor) { mImpl->renderer.setExternalShadowViewProj(matrix4x4ColumnMajor); }

void SkyAtmosphereRenderer::setAerialPerspectiveDebugDepthKm(float depthKm) { mImpl->renderer.setAerialPerspectiveDebugDepthKm(depthKm); }
void SkyAtmosphereRenderer::setCameraHeight(float value) { mImpl->renderer.setCameraHeight(value); }
void SkyAtmosphereRenderer::setCameraForward(float value) { mImpl->renderer.setCameraForward(value); }
void SkyAtmosphereRenderer::setCameraOffset(const Vec3& value) { mImpl->renderer.setCameraOffset(toInternal(value)); }
void SkyAtmosphereRenderer::setViewYaw(float value) { mImpl->renderer.setViewYaw(value); }
void SkyAtmosphereRenderer::setViewPitch(float value) { mImpl->renderer.setViewPitch(value); }
void SkyAtmosphereRenderer::setSunIlluminanceScale(float value) { mImpl->renderer.setSunIlluminanceScale(value); }
void SkyAtmosphereRenderer::setSunYaw(float value) { mImpl->renderer.setSunYaw(value); }
void SkyAtmosphereRenderer::setSunPitch(float value) { mImpl->renderer.setSunPitch(value); }
void SkyAtmosphereRenderer::setRayMarchMinSpp(int value) { mImpl->renderer.setRayMarchMinSpp(value); }
void SkyAtmosphereRenderer::setRayMarchMaxSpp(int value) { mImpl->renderer.setRayMarchMaxSpp(value); }
void SkyAtmosphereRenderer::setFastSky(bool enabled) { mImpl->renderer.setFastSky(enabled); }
void SkyAtmosphereRenderer::setFastAerialPerspective(bool enabled) { mImpl->renderer.setFastAerialPerspective(enabled); }
void SkyAtmosphereRenderer::setShadowMapsEnabled(bool enabled) { mImpl->renderer.setShadowMapsEnabled(enabled); }
void SkyAtmosphereRenderer::setColoredTransmittance(bool enabled) { mImpl->renderer.setColoredTransmittance(enabled); }
void SkyAtmosphereRenderer::setAerialPerspectivePreviewSlice(int value) { mImpl->renderer.setAerialPerspectivePreviewSlice(value); }
void SkyAtmosphereRenderer::setMultiScatteringPreviewExposure(float value) { mImpl->renderer.setMultiScatteringPreviewExposure(value); }
void SkyAtmosphereRenderer::setAerialPerspectivePreviewExposure(float value) { mImpl->renderer.setAerialPerspectivePreviewExposure(value); }
void SkyAtmosphereRenderer::setUseAgxTonemap(bool enabled) { mImpl->renderer.setUseAgxTonemap(enabled); }
void SkyAtmosphereRenderer::setAutoExposureEnabled(bool enabled) { mImpl->renderer.setAutoExposureEnabled(enabled); }
void SkyAtmosphereRenderer::setManualExposure(float value) { mImpl->renderer.setManualExposure(value); }
void SkyAtmosphereRenderer::setExposureBiasEv(float value) { mImpl->renderer.setExposureBiasEv(value); }
void SkyAtmosphereRenderer::setUseHistogramAutoExposure(bool enabled) { mImpl->renderer.setUseHistogramAutoExposure(enabled); }
void SkyAtmosphereRenderer::setAutoExposureHistogramLowPercent(float value) { mImpl->renderer.setAutoExposureHistogramLowPercent(value); }
void SkyAtmosphereRenderer::setAutoExposureHistogramHighPercent(float value) { mImpl->renderer.setAutoExposureHistogramHighPercent(value); }
void SkyAtmosphereRenderer::setSunAngleExposureBiasEnabled(bool enabled) { mImpl->renderer.setSunAngleExposureBiasEnabled(enabled); }
void SkyAtmosphereRenderer::setSunAngleExposureBiasAtHorizonEv(float value) { mImpl->renderer.setSunAngleExposureBiasAtHorizonEv(value); }
void SkyAtmosphereRenderer::setSunAngleExposureBiasAtNoonEv(float value) { mImpl->renderer.setSunAngleExposureBiasAtNoonEv(value); }
void SkyAtmosphereRenderer::setDisplayGamma(float value) { mImpl->renderer.setDisplayGamma(value); }
void SkyAtmosphereRenderer::setAutoExposureKey(float value) { mImpl->renderer.setAutoExposureKey(value); }
void SkyAtmosphereRenderer::setAgxSaturation(float value) { mImpl->renderer.setAgxSaturation(value); }
void SkyAtmosphereRenderer::setPhysicalModeEnabled(bool enabled) { mImpl->renderer.setPhysicalModeEnabled(enabled); }
void SkyAtmosphereRenderer::setCameraEv100(float value) { mImpl->renderer.setCameraEv100(value); }
void SkyAtmosphereRenderer::setOutputSrgb(bool enabled) { mImpl->renderer.setOutputSrgb(enabled); }
void SkyAtmosphereRenderer::setMultipleScatteringFactor(float value) { mImpl->renderer.setMultipleScatteringFactor(value); }
void SkyAtmosphereRenderer::setAtmosphereInfo(const AtmosphereInfo& value) { mImpl->renderer.setAtmosphereInfo(toInternal(value)); }

float SkyAtmosphereRenderer::getCameraHeight() const { return mImpl->renderer.getCameraHeight(); }
float SkyAtmosphereRenderer::getCameraForward() const { return mImpl->renderer.getCameraForward(); }
Vec3 SkyAtmosphereRenderer::getCameraOffset() const { return toPublic(mImpl->renderer.getCameraOffset()); }
float SkyAtmosphereRenderer::getViewYaw() const { return mImpl->renderer.getViewYaw(); }
float SkyAtmosphereRenderer::getViewPitch() const { return mImpl->renderer.getViewPitch(); }
Vec3 SkyAtmosphereRenderer::getViewDir() const { return toPublic(mImpl->renderer.getViewDir()); }
Vec3 SkyAtmosphereRenderer::getViewRight() const { return toPublic(mImpl->renderer.getViewRight()); }
Vec3 SkyAtmosphereRenderer::getViewUp() const { return toPublic(mImpl->renderer.getViewUp()); }
float SkyAtmosphereRenderer::getSunIlluminanceScale() const { return mImpl->renderer.getSunIlluminanceScale(); }
float SkyAtmosphereRenderer::getSunYaw() const { return mImpl->renderer.getSunYaw(); }
float SkyAtmosphereRenderer::getSunPitch() const { return mImpl->renderer.getSunPitch(); }
int SkyAtmosphereRenderer::getRayMarchMinSpp() const { return mImpl->renderer.getRayMarchMinSpp(); }
int SkyAtmosphereRenderer::getRayMarchMaxSpp() const { return mImpl->renderer.getRayMarchMaxSpp(); }
bool SkyAtmosphereRenderer::getFastSky() const { return mImpl->renderer.getFastSky(); }
bool SkyAtmosphereRenderer::getFastAerialPerspective() const { return mImpl->renderer.getFastAerialPerspective(); }
bool SkyAtmosphereRenderer::getShadowMapsEnabled() const { return mImpl->renderer.getShadowMapsEnabled(); }
bool SkyAtmosphereRenderer::getColoredTransmittance() const { return mImpl->renderer.getColoredTransmittance(); }
float SkyAtmosphereRenderer::getMultipleScatteringFactor() const { return mImpl->renderer.getMultipleScatteringFactor(); }
bool SkyAtmosphereRenderer::getUseAgxTonemap() const { return mImpl->renderer.getUseAgxTonemap(); }
bool SkyAtmosphereRenderer::getAutoExposureEnabled() const { return mImpl->renderer.getAutoExposureEnabled(); }
float SkyAtmosphereRenderer::getManualExposure() const { return mImpl->renderer.getManualExposure(); }
float SkyAtmosphereRenderer::getExposureBiasEv() const { return mImpl->renderer.getExposureBiasEv(); }
bool SkyAtmosphereRenderer::getUseHistogramAutoExposure() const { return mImpl->renderer.getUseHistogramAutoExposure(); }
float SkyAtmosphereRenderer::getAutoExposureHistogramLowPercent() const { return mImpl->renderer.getAutoExposureHistogramLowPercent(); }
float SkyAtmosphereRenderer::getAutoExposureHistogramHighPercent() const { return mImpl->renderer.getAutoExposureHistogramHighPercent(); }
bool SkyAtmosphereRenderer::getSunAngleExposureBiasEnabled() const { return mImpl->renderer.getSunAngleExposureBiasEnabled(); }
float SkyAtmosphereRenderer::getSunAngleExposureBiasAtHorizonEv() const { return mImpl->renderer.getSunAngleExposureBiasAtHorizonEv(); }
float SkyAtmosphereRenderer::getSunAngleExposureBiasAtNoonEv() const { return mImpl->renderer.getSunAngleExposureBiasAtNoonEv(); }
float SkyAtmosphereRenderer::getDisplayGamma() const { return mImpl->renderer.getDisplayGamma(); }
float SkyAtmosphereRenderer::getAutoExposureKey() const { return mImpl->renderer.getAutoExposureKey(); }
float SkyAtmosphereRenderer::getAgxSaturation() const { return mImpl->renderer.getAgxSaturation(); }
bool SkyAtmosphereRenderer::getPhysicalModeEnabled() const { return mImpl->renderer.getPhysicalModeEnabled(); }
float SkyAtmosphereRenderer::getCameraEv100() const { return mImpl->renderer.getCameraEv100(); }
bool SkyAtmosphereRenderer::getOutputSrgb() const { return mImpl->renderer.getOutputSrgb(); }
AtmosphereInfo SkyAtmosphereRenderer::getAtmosphereInfo() const { return toPublic(mImpl->renderer.getAtmosphereInfo()); }

unsigned int SkyAtmosphereRenderer::getTransmittanceTexture() const { return mImpl->renderer.getTransmittanceTexture(); }
unsigned int SkyAtmosphereRenderer::getMultipleScatteringPreviewTexture() const { return mImpl->renderer.getMultipleScatteringPreviewTexture(); }
unsigned int SkyAtmosphereRenderer::getSkyViewTexture() const { return mImpl->renderer.getSkyViewTexture(); }
unsigned int SkyAtmosphereRenderer::getAerialPerspectivePreviewTexture() const { return mImpl->renderer.getAerialPerspectivePreviewTexture(); }
int SkyAtmosphereRenderer::getAerialPerspectiveDepthSliceCount() const { return mImpl->renderer.getAerialPerspectiveDepthSliceCount(); }
int SkyAtmosphereRenderer::getAerialPerspectivePreviewSlice() const { return mImpl->renderer.getAerialPerspectivePreviewSlice(); }
float SkyAtmosphereRenderer::getMultiScatteringDebugMin() const { return mImpl->renderer.getMultiScatteringDebugMin(); }
float SkyAtmosphereRenderer::getMultiScatteringDebugMax() const { return mImpl->renderer.getMultiScatteringDebugMax(); }
bool SkyAtmosphereRenderer::hasMultiScatteringDebugStats() const { return mImpl->renderer.hasMultiScatteringDebugStats(); }
float SkyAtmosphereRenderer::getAerialPerspectiveDebugMin() const { return mImpl->renderer.getAerialPerspectiveDebugMin(); }
float SkyAtmosphereRenderer::getAerialPerspectiveDebugMax() const { return mImpl->renderer.getAerialPerspectiveDebugMax(); }
bool SkyAtmosphereRenderer::hasAerialPerspectiveDebugStats() const { return mImpl->renderer.hasAerialPerspectiveDebugStats(); }
bool SkyAtmosphereRenderer::isInitialised() const { return mImpl->renderer.isInitialised(); }
bool SkyAtmosphereRenderer::hasGpuPassTimings() const { return mImpl->renderer.hasGpuPassTimings(); }
float SkyAtmosphereRenderer::getTransmittancePassMs() const { return mImpl->renderer.getTransmittancePassMs(); }
float SkyAtmosphereRenderer::getMultiScatteringPassMs() const { return mImpl->renderer.getMultiScatteringPassMs(); }
float SkyAtmosphereRenderer::getSkyViewPassMs() const { return mImpl->renderer.getSkyViewPassMs(); }
float SkyAtmosphereRenderer::getAerialPerspectivePassMs() const { return mImpl->renderer.getAerialPerspectivePassMs(); }
float SkyAtmosphereRenderer::getPresentPassMs() const { return mImpl->renderer.getPresentPassMs(); }

}
