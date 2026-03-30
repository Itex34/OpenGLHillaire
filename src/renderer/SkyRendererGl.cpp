// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkyRendererGl.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#include <GL/gl3w.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <limits>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	const char* kFullscreenVertexShaderPath = "Resources/glsl/fullscreen_triangle.vert";
	const char* kTransmittanceFragmentShaderPath = "Resources/glsl/transmittance_lut.frag";
	const char* kMultiScatteringComputeShaderPath = "Resources/glsl/new_multi_scattering_lut.comp";
	const char* kSkyViewFragmentShaderPath = "Resources/glsl/skyview_lut.frag";
	const char* kAerialPerspectiveComputeShaderPath = "Resources/glsl/aerial_perspective_volume.comp";
	const char* kRaymarchFragmentShaderPath = "Resources/glsl/render_raymarching_hillaire.frag";
	const char* kPostProcessFragmentShaderPath = "Resources/glsl/postprocess.frag";
	const char* kAutoExposureHistogramComputeShaderPath = "Resources/glsl/auto_exposure_histogram.comp";
	const char* kAutoExposureReduceComputeShaderPath = "Resources/glsl/auto_exposure_reduce.comp";
	const float kMainCameraFovYDegrees = 66.6f;
	const int kAutoExposureHistogramBinCount = 128;

	float toneMapPreviewValue(float value, float exposure)
	{
		const float linear = value > 0.0f ? (value * exposure) : 0.0f;
		const float reinhard = linear / (1.0f + linear);
		return reinhard;
	}

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

	void setupEarthAtmosphere(GlAtmosphereInfo& info)
	{
		const float earthBottomRadius = 6360.0f;
		const float earthTopRadius = 6460.0f;
		const float earthRayleighScaleHeight = 8.0f;
		const float earthMieScaleHeight = 1.2f;

		info.bottom_radius = earthBottomRadius;
		info.top_radius = earthTopRadius;
		info.rayleigh_scattering = { 0.005802f, 0.013558f, 0.033100f };
		info.mie_scattering = { 0.003996f, 0.003996f, 0.003996f };
		info.mie_extinction = { 0.004440f, 0.004440f, 0.004440f };
		info.mie_absorption = {
			info.mie_extinction.x - info.mie_scattering.x,
			info.mie_extinction.y - info.mie_scattering.y,
			info.mie_extinction.z - info.mie_scattering.z,
		};
		info.mie_phase_g = 0.8f;
		info.absorption_extinction = { 0.000650f, 0.001881f, 0.000085f };
		info.ground_albedo = { 0.0f, 0.0f, 0.0f };
		info.rayleigh_density_exp_scale = -1.0f / earthRayleighScaleHeight;
		info.mie_density_exp_scale = -1.0f / earthMieScaleHeight;
		info.absorption_layer0_width = 25.0f;
		info.absorption_layer0_linear_term = 1.0f / 15.0f;
		info.absorption_layer0_constant_term = -2.0f / 3.0f;
		info.absorption_layer1_linear_term = -1.0f / 15.0f;
		info.absorption_layer1_constant_term = 8.0f / 3.0f;
	}

	glm::vec3 makeViewDirectionZUpForwardY(float yaw, float pitch)
	{
		const float cp = std::cos(pitch);
		const glm::vec3 dir(
			std::sin(yaw) * cp,
			std::cos(yaw) * cp,
			std::sin(pitch));
		const float len = glm::length(dir);
		return (len > 1e-6f) ? (dir / len) : glm::vec3(0.0f, 1.0f, 0.0f);
	}

	int computeLastMipLevel(int width, int height)
	{
		int w = width > 1 ? width : 1;
		int h = height > 1 ? height : 1;
		int level = 0;
		while (w > 1 || h > 1)
		{
			w = w > 1 ? (w / 2) : 1;
			h = h > 1 ? (h / 2) : 1;
			++level;
		}
		return level;
	}

}


void SkyRendererGl::markLutsDirty()
{
	mLutDirty = true;
	mSkyViewDirty = true;
	mAerialPerspectiveDirty = true;
}

void SkyRendererGl::markSkyAndApDirty()
{
	mSkyViewDirty = true;
	mAerialPerspectiveDirty = true;
}

void SkyRendererGl::setExternalSceneTextures(unsigned int hdrTexture, unsigned int linearDepthTexture)
{
	if (hdrTexture == 0 || linearDepthTexture == 0)
	{
		clearExternalSceneTextures();
		return;
	}
	mExternalSceneHdrTex = hdrTexture;
	mExternalSceneLinearDepthTex = linearDepthTexture;
	mHasExternalSceneTextures = true;
}

void SkyRendererGl::clearExternalSceneTextures()
{
	mHasExternalSceneTextures = false;
	mExternalSceneHdrTex = 0;
	mExternalSceneLinearDepthTex = 0;
}

void SkyRendererGl::setExternalShadowMapTexture(unsigned int depthCompareTexture)
{
	if (depthCompareTexture == 0)
	{
		clearExternalShadowMapTexture();
		return;
	}
	mExternalShadowMapDepthTex = depthCompareTexture;
	mHasExternalShadowMapTexture = true;
}

void SkyRendererGl::clearExternalShadowMapTexture()
{
	mHasExternalShadowMapTexture = false;
	mExternalShadowMapDepthTex = 0;
}

void SkyRendererGl::setExternalShadowViewProj(const float* matrix4x4ColumnMajor)
{
	if (matrix4x4ColumnMajor == nullptr)
	{
		return;
	}
	std::memcpy(mShadowViewProj, matrix4x4ColumnMajor, sizeof(mShadowViewProj));
}

void SkyRendererGl::setCameraHeight(float value)
{
	if (std::fabs(mCameraHeight - value) > 1e-6f)
	{
		mCameraHeight = value;
		mCameraPosition.z = value;
		markSkyAndApDirty();
	}
}

void SkyRendererGl::setCameraForward(float value)
{
	if (std::fabs(mCameraForward - value) > 1e-6f)
	{
		mCameraForward = value;
		mCameraPosition.y = value;
		markSkyAndApDirty();
	}
}

void SkyRendererGl::setCameraOffset(const GlVec3& value)
{
	const bool changed =
		std::fabs(mCameraPosition.x - value.x) > 1e-6f ||
		std::fabs(mCameraPosition.y - value.y) > 1e-6f ||
		std::fabs(mCameraPosition.z - value.z) > 1e-6f;
	if (changed)
	{
		mCameraPosition = value;
		mCameraForward = value.y;
		mCameraHeight = value.z;
		markSkyAndApDirty();
		updateViewAndSunDirections();
	}
}

void SkyRendererGl::setViewYaw(float value)
{
	if (std::fabs(mViewYaw - value) > 1e-6f)
	{
		mViewYaw = value;
		mAerialPerspectiveDirty = true;
		updateViewAndSunDirections();
	}
}

void SkyRendererGl::setViewPitch(float value)
{
	const float clamped = value < -1.55f ? -1.55f : (value > 1.55f ? 1.55f : value);
	if (std::fabs(mViewPitch - clamped) > 1e-6f)
	{
		mViewPitch = clamped;
		mAerialPerspectiveDirty = true;
		updateViewAndSunDirections();
	}
}

void SkyRendererGl::setSunIlluminanceScale(float value)
{
	if (std::fabs(mSunIlluminanceScale - value) > 1e-6f)
	{
		mSunIlluminanceScale = value;
		markSkyAndApDirty();
	}
}

void SkyRendererGl::setSunYaw(float value)
{
	if (std::fabs(mSunYaw - value) > 1e-6f)
	{
		mSunYaw = value;
		markSkyAndApDirty();
	}
}

void SkyRendererGl::setSunPitch(float value)
{
	if (std::fabs(mSunPitch - value) > 1e-6f)
	{
		mSunPitch = value;
		markSkyAndApDirty();
	}
}

void SkyRendererGl::setRayMarchMinSpp(int value)
{
	const int clamped = value < 1 ? 1 : value;
	if (mRayMarchMinSpp != clamped)
	{
		mRayMarchMinSpp = clamped;
		if (mRayMarchMaxSpp <= mRayMarchMinSpp)
		{
			mRayMarchMaxSpp = mRayMarchMinSpp + 1;
		}
	}
}

void SkyRendererGl::setRayMarchMaxSpp(int value)
{
	int clamped = value < 2 ? 2 : value;
	if (clamped <= mRayMarchMinSpp)
	{
		clamped = mRayMarchMinSpp + 1;
	}
	if (mRayMarchMaxSpp != clamped)
	{
		mRayMarchMaxSpp = clamped;
	}
}

void SkyRendererGl::setFastSky(bool enabled)
{
	mFastSky = enabled;
}

void SkyRendererGl::setFastAerialPerspective(bool enabled)
{
	mFastAerialPerspective = enabled;
}

void SkyRendererGl::setAerialPerspectivePreviewSlice(int value)
{
	const int depth = static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH);
	if (depth <= 0)
	{
		mAerialPerspectivePreviewSlice = 0;
		return;
	}
	int clamped = value;
	if (clamped < 0) clamped = 0;
	if (clamped >= depth) clamped = depth - 1;
	mAerialPerspectivePreviewSlice = clamped;
}

void SkyRendererGl::setManualExposure(float value)
{
	const float clamped = value < 0.001f ? 0.001f : value;
	mManualExposure = clamped;
}

void SkyRendererGl::setExposureBiasEv(float value)
{
	const float clamped = value < -16.0f ? -16.0f : (value > 16.0f ? 16.0f : value);
	mExposureBiasEv = clamped;
}

void SkyRendererGl::setAutoExposureHistogramLowPercent(float value)
{
	float clamped = value < 0.0f ? 0.0f : (value > 95.0f ? 95.0f : value);
	if (clamped >= mAutoExposureHistogramHighPercent)
	{
		clamped = mAutoExposureHistogramHighPercent - 1.0f;
	}
	mAutoExposureHistogramLowPercent = clamped < 0.0f ? 0.0f : clamped;
}

void SkyRendererGl::setAutoExposureHistogramHighPercent(float value)
{
	float clamped = value < 5.0f ? 5.0f : (value > 100.0f ? 100.0f : value);
	if (clamped <= mAutoExposureHistogramLowPercent)
	{
		clamped = mAutoExposureHistogramLowPercent + 1.0f;
	}
	mAutoExposureHistogramHighPercent = clamped > 100.0f ? 100.0f : clamped;
}

void SkyRendererGl::setSunAngleExposureBiasAtHorizonEv(float value)
{
	const float clamped = value < -16.0f ? -16.0f : (value > 16.0f ? 16.0f : value);
	mSunAngleExposureBiasAtHorizonEv = clamped;
}

void SkyRendererGl::setSunAngleExposureBiasAtNoonEv(float value)
{
	const float clamped = value < -16.0f ? -16.0f : (value > 16.0f ? 16.0f : value);
	mSunAngleExposureBiasAtNoonEv = clamped;
}

void SkyRendererGl::setDisplayGamma(float value)
{
	const float clamped = value < 0.01f ? 0.01f : value;
	mDisplayGamma = clamped;
}

void SkyRendererGl::setAutoExposureKey(float value)
{
	const float clamped = value < 0.01f ? 0.01f : (value > 1.0f ? 1.0f : value);
	mAutoExposureKey = clamped;
}

void SkyRendererGl::setAgxSaturation(float value)
{
	const float clamped = value < 0.0f ? 0.0f : (value > 2.0f ? 2.0f : value);
	mAgxSaturation = clamped;
}

void SkyRendererGl::setCameraEv100(float value)
{
	const float clamped = value < -6.0f ? -6.0f : (value > 24.0f ? 24.0f : value);
	mCameraEv100 = clamped;
}

void SkyRendererGl::setMultipleScatteringFactor(float value)
{
	if (std::fabs(mMultipleScatteringFactor - value) > 1e-6f)
	{
		mMultipleScatteringFactor = value;
		markLutsDirty();
	}
}

void SkyRendererGl::setAtmosphereInfo(const GlAtmosphereInfo& value)
{
	mAtmosphereInfo = value;
	markLutsDirty();
}

void SkyRendererGl::updateViewAndSunDirections()
{
	const glm::vec3 viewDir = makeViewDirectionZUpForwardY(mViewYaw, mViewPitch);
	mViewDir = { viewDir.x, viewDir.y, viewDir.z };

	const glm::vec3 worldUp(0.0f, 0.0f, 1.0f);
	glm::vec3 viewRight = glm::cross(worldUp, viewDir);
	if (glm::length(viewRight) < 1e-6f)
	{
		viewRight = glm::vec3(1.0f, 0.0f, 0.0f);
	}
	else
	{
		viewRight = glm::normalize(viewRight);
	}
	glm::vec3 viewUp = glm::cross(viewDir, viewRight);
	if (glm::length(viewUp) < 1e-6f)
	{
		viewUp = worldUp;
	}
	else
	{
		viewUp = glm::normalize(viewUp);
	}
	mViewRight = { viewRight.x, viewRight.y, viewRight.z };
	mViewUp = { viewUp.x, viewUp.y, viewUp.z };

	mCameraOffset = mCameraPosition;
	mCameraForward = mCameraPosition.y;
	mCameraHeight = mCameraPosition.z;

	const glm::vec3 sunDir = makeViewDirectionZUpForwardY(mSunYaw, mSunPitch);
	mSunDir = { sunDir.x, sunDir.y, sunDir.z };
}

void SkyRendererGl::createGpuPassTimers()
{
	mGpuPassTimingsSupported = true;

	GpuPassTimer* timers[] = {
		&mTransmittancePassTimer,
		&mMultiScatteringPassTimer,
		&mSkyViewPassTimer,
		&mAerialPerspectivePassTimer,
		&mPresentPassTimer
	};

	for (GpuPassTimer* timer : timers)
	{
		timer->query = 0;
		timer->lastMs = 0.0f;
		timer->pending = false;
		timer->active = false;
		glGenQueries(1, &timer->query);
	}

	const unsigned int err = glGetError();
	if (err != GL_NO_ERROR)
	{
		destroyGpuPassTimers();
		mGpuPassTimingsSupported = false;
	}
}

void SkyRendererGl::destroyGpuPassTimers()
{
	GpuPassTimer* timers[] = {
		&mTransmittancePassTimer,
		&mMultiScatteringPassTimer,
		&mSkyViewPassTimer,
		&mAerialPerspectivePassTimer,
		&mPresentPassTimer
	};

	for (GpuPassTimer* timer : timers)
	{
		if (timer->query != 0)
		{
			glDeleteQueries(1, &timer->query);
			timer->query = 0;
		}
		timer->lastMs = 0.0f;
		timer->pending = false;
		timer->active = false;
	}

	mGpuPassTimingsSupported = false;
}

void SkyRendererGl::resolveGpuPassTimers()
{
	if (!mGpuPassTimingsSupported)
	{
		return;
	}

	GpuPassTimer* timers[] = {
		&mTransmittancePassTimer,
		&mMultiScatteringPassTimer,
		&mSkyViewPassTimer,
		&mAerialPerspectivePassTimer,
		&mPresentPassTimer
	};

	for (GpuPassTimer* timer : timers)
	{
		if (timer->query == 0 || !timer->pending)
		{
			continue;
		}
		unsigned int available = 0;
		glGetQueryObjectuiv(timer->query, GL_QUERY_RESULT_AVAILABLE, &available);
		if (available == 0)
		{
			continue;
		}
		GLuint64 elapsedNs = 0;
		glGetQueryObjectui64v(timer->query, GL_QUERY_RESULT, &elapsedNs);
		timer->lastMs = static_cast<float>(elapsedNs * (1.0 / 1000000.0));
		timer->pending = false;
	}
}

void SkyRendererGl::beginGpuPassTimer(GpuPassTimer& timer)
{
	if (!mGpuPassTimingsSupported || timer.query == 0 || timer.pending || timer.active)
	{
		return;
	}
	glBeginQuery(GL_TIME_ELAPSED, timer.query);
	timer.active = true;
}

void SkyRendererGl::endGpuPassTimer(GpuPassTimer& timer)
{
	if (!mGpuPassTimingsSupported || !timer.active)
	{
		return;
	}
	glEndQuery(GL_TIME_ELAPSED);
	timer.active = false;
	timer.pending = true;
}

unsigned int SkyRendererGl::loadAndCompileShader(unsigned int type, const char* path)
{
	const std::string source = readTextFile(path);
	if (source.empty())
	{
		showMessageBox("OpenGL shader error", std::string("Failed to read shader file: ") + path);
		return 0;
	}

	const char* srcPtr = source.c_str();
	unsigned int shader = glCreateShader(type);
	glShaderSource(shader, 1, &srcPtr, nullptr);
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
	log.resize(logLength > 1 ? size_t(logLength) : 1);
	glGetShaderInfoLog(shader, logLength, nullptr, &log[0]);
	showMessageBox("OpenGL shader compile error", std::string("File: ") + path + "\n\n" + log);
	glDeleteShader(shader);
	return 0;
}

unsigned int SkyRendererGl::linkProgram(unsigned int vs, unsigned int fs, const char* debugName)
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
	log.resize(logLength > 1 ? size_t(logLength) : 1);
	glGetProgramInfoLog(program, logLength, nullptr, &log[0]);
	showMessageBox("OpenGL program link error", std::string("Program: ") + debugName + "\n\n" + log);
	glDeleteProgram(program);
	return 0;
}

unsigned int SkyRendererGl::linkComputeProgram(unsigned int cs, const char* debugName)
{
	unsigned int program = glCreateProgram();
	glAttachShader(program, cs);
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
	log.resize(logLength > 1 ? size_t(logLength) : 1);
	glGetProgramInfoLog(program, logLength, nullptr, &log[0]);
	showMessageBox("OpenGL program link error", std::string("Program: ") + debugName + "\n\n" + log);
	glDeleteProgram(program);
	return 0;
}

bool SkyRendererGl::createPrograms()
{
	const unsigned int fullscreenVs = loadAndCompileShader(GL_VERTEX_SHADER, kFullscreenVertexShaderPath);
	const unsigned int transmittanceFs = loadAndCompileShader(GL_FRAGMENT_SHADER, kTransmittanceFragmentShaderPath);
	const unsigned int multiScatteringCs = loadAndCompileShader(GL_COMPUTE_SHADER, kMultiScatteringComputeShaderPath);
	const unsigned int skyViewFs = loadAndCompileShader(GL_FRAGMENT_SHADER, kSkyViewFragmentShaderPath);
	const unsigned int aerialPerspectiveCs = loadAndCompileShader(GL_COMPUTE_SHADER, kAerialPerspectiveComputeShaderPath);
	const unsigned int raymarchFs = loadAndCompileShader(GL_FRAGMENT_SHADER, kRaymarchFragmentShaderPath);
	const unsigned int postProcessFs = loadAndCompileShader(GL_FRAGMENT_SHADER, kPostProcessFragmentShaderPath);
	const unsigned int autoExposureHistogramCs = loadAndCompileShader(GL_COMPUTE_SHADER, kAutoExposureHistogramComputeShaderPath);
	const unsigned int autoExposureReduceCs = loadAndCompileShader(GL_COMPUTE_SHADER, kAutoExposureReduceComputeShaderPath);

	if (fullscreenVs == 0 || transmittanceFs == 0 || multiScatteringCs == 0 || skyViewFs == 0 || aerialPerspectiveCs == 0 || raymarchFs == 0 || postProcessFs == 0 || autoExposureHistogramCs == 0 || autoExposureReduceCs == 0)
	{
		if (fullscreenVs != 0) glDeleteShader(fullscreenVs);
		if (transmittanceFs != 0) glDeleteShader(transmittanceFs);
		if (multiScatteringCs != 0) glDeleteShader(multiScatteringCs);
		if (skyViewFs != 0) glDeleteShader(skyViewFs);
		if (aerialPerspectiveCs != 0) glDeleteShader(aerialPerspectiveCs);
		if (raymarchFs != 0) glDeleteShader(raymarchFs);
		if (postProcessFs != 0) glDeleteShader(postProcessFs);
		if (autoExposureHistogramCs != 0) glDeleteShader(autoExposureHistogramCs);
		if (autoExposureReduceCs != 0) glDeleteShader(autoExposureReduceCs);
		return false;
	}

	mTransmittanceProgram = linkProgram(fullscreenVs, transmittanceFs, "TransmittanceLut");
	mMultiScatteringProgram = linkComputeProgram(multiScatteringCs, "NewMultiScatteringLut");
	mSkyViewProgram = linkProgram(fullscreenVs, skyViewFs, "SkyViewLut");
	mAerialPerspectiveProgram = linkComputeProgram(aerialPerspectiveCs, "AerialPerspectiveVolume");
	mRaymarchProgram = linkProgram(fullscreenVs, raymarchFs, "RenderRaymarchingHillaire");
	mPostProcessProgram = linkProgram(fullscreenVs, postProcessFs, "PostProcess");
	mAutoExposureHistogramProgram = linkComputeProgram(autoExposureHistogramCs, "AutoExposureHistogram");
	mAutoExposureReduceProgram = linkComputeProgram(autoExposureReduceCs, "AutoExposureReduce");

	glDeleteShader(fullscreenVs);
	glDeleteShader(transmittanceFs);
	glDeleteShader(multiScatteringCs);
	glDeleteShader(skyViewFs);
	glDeleteShader(aerialPerspectiveCs);
	glDeleteShader(raymarchFs);
	glDeleteShader(postProcessFs);
	glDeleteShader(autoExposureHistogramCs);
	glDeleteShader(autoExposureReduceCs);

	if (mTransmittanceProgram == 0 || mMultiScatteringProgram == 0 || mSkyViewProgram == 0 || mAerialPerspectiveProgram == 0 || mRaymarchProgram == 0 || mPostProcessProgram == 0 || mAutoExposureHistogramProgram == 0 || mAutoExposureReduceProgram == 0)
	{
		destroyPrograms();
		return false;
	}

	return true;
}

bool SkyRendererGl::createAutoExposureResources()
{
	glGenBuffers(1, &mAutoExposureHistogramSsbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mAutoExposureHistogramSsbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(unsigned int) * static_cast<size_t>(kAutoExposureHistogramBinCount), nullptr, GL_DYNAMIC_DRAW);
	unsigned int clearValue = 0u;
	glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &clearValue);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glGenTextures(1, &mAutoExposureMeterTex);
	glBindTexture(GL_TEXTURE_2D, mAutoExposureMeterTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	const float initialMeterLuminance = 0.18f;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &initialMeterLuminance);
	glBindTexture(GL_TEXTURE_2D, 0);

	const unsigned int err = glGetError();
	if (err != GL_NO_ERROR)
	{
		showMessageBox("OpenGL resource error", "Failed to create auto-exposure histogram resources.");
		destroyAutoExposureResources();
		return false;
	}
	return true;
}

bool SkyRendererGl::createTransmittanceResources()
{
	glGenTextures(1, &mTransmittanceTex);
	glBindTexture(GL_TEXTURE_2D, mTransmittanceTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA16F,
		static_cast<GLsizei>(mLutsInfo.TRANSMITTANCE_TEXTURE_WIDTH),
		static_cast<GLsizei>(mLutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT),
		0,
		GL_RGBA,
		GL_FLOAT,
		nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &mTransmittanceFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mTransmittanceFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTransmittanceTex, 0);
	const unsigned int drawBuffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &drawBuffer);

	const unsigned int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		showMessageBox("OpenGL framebuffer error", "Failed to create transmittance LUT framebuffer.");
		destroyTransmittanceResources();
		return false;
	}
	return true;
}

bool SkyRendererGl::createMultipleScatteringResources()
{
	const GLsizei lutSize = static_cast<GLsizei>(mLutsInfo.MULTI_SCATTERING_TEXTURE_SIZE);

	glGenTextures(1, &mMultiScatteringTex);
	glBindTexture(GL_TEXTURE_2D, mMultiScatteringTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA32F,
		lutSize,
		lutSize,
		0,
		GL_RGBA,
		GL_FLOAT,
		nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &mMultiScatteringPreviewTex);
	glBindTexture(GL_TEXTURE_2D, mMultiScatteringPreviewTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, lutSize, lutSize, 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
	return true;
}

bool SkyRendererGl::createSkyViewResources()
{
	const int skyWidth = static_cast<int>(mLutsInfo.SKY_VIEW_TEXTURE_WIDTH);
	const int skyHeight = static_cast<int>(mLutsInfo.SKY_VIEW_TEXTURE_HEIGHT);

	glGenTextures(1, &mSkyViewTex);
	glBindTexture(GL_TEXTURE_2D, mSkyViewTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, skyWidth, skyHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &mSkyViewFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mSkyViewFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSkyViewTex, 0);
	const unsigned int drawBuffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &drawBuffer);

	const unsigned int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		showMessageBox("OpenGL framebuffer error", "Failed to create sky-view LUT framebuffer.");
		destroySkyViewResources();
		return false;
	}
	return true;
}

bool SkyRendererGl::createAerialPerspectiveResources()
{
	const GLsizei apWidth = static_cast<GLsizei>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_WIDTH);
	const GLsizei apHeight = static_cast<GLsizei>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_HEIGHT);
	const GLsizei apDepth = static_cast<GLsizei>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH);

	glGenTextures(1, &mAerialPerspectiveTex);
	glBindTexture(GL_TEXTURE_3D, mAerialPerspectiveTex);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexImage3D(
		GL_TEXTURE_3D,
		0,
		GL_RGBA16F,
		apWidth,
		apHeight,
		apDepth,
		0,
		GL_RGBA,
		GL_FLOAT,
		nullptr);
	glBindTexture(GL_TEXTURE_3D, 0);

	glGenTextures(1, &mAerialPerspectivePreviewTex);
	glBindTexture(GL_TEXTURE_2D, mAerialPerspectivePreviewTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, apWidth, apHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	mAerialPerspectivePreviewSlice = static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH / 2u);
	return true;
}

bool SkyRendererGl::createShadowResources()
{
	// 1x1 always-lit fallback for when no external shadow map is supplied.
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
	return true;
}

bool SkyRendererGl::createSceneResources()
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
		showMessageBox("OpenGL framebuffer error", "Failed to create scene framebuffer.");
		destroySceneResources();
		return false;
	}

	glGenTextures(1, &mFinalHdrTex);
	glBindTexture(GL_TEXTURE_2D, mFinalHdrTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	mFinalHdrMipLevel = computeLastMipLevel(mBackbufferWidth, mBackbufferHeight);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mFinalHdrMipLevel);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &mFinalHdrFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, mFinalHdrFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mFinalHdrTex, 0);
	const unsigned int finalDrawBuffer = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &finalDrawBuffer);
	const unsigned int finalStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (finalStatus != GL_FRAMEBUFFER_COMPLETE)
	{
		showMessageBox("OpenGL framebuffer error", "Failed to create final HDR framebuffer.");
		destroySceneResources();
		return false;
	}
	return true;
}

void SkyRendererGl::destroyPrograms()
{
	if (mAutoExposureReduceProgram != 0)
	{
		glDeleteProgram(mAutoExposureReduceProgram);
		mAutoExposureReduceProgram = 0;
	}
	if (mAutoExposureHistogramProgram != 0)
	{
		glDeleteProgram(mAutoExposureHistogramProgram);
		mAutoExposureHistogramProgram = 0;
	}
	if (mPostProcessProgram != 0)
	{
		glDeleteProgram(mPostProcessProgram);
		mPostProcessProgram = 0;
	}
	if (mRaymarchProgram != 0)
	{
		glDeleteProgram(mRaymarchProgram);
		mRaymarchProgram = 0;
	}
	if (mSkyViewProgram != 0)
	{
		glDeleteProgram(mSkyViewProgram);
		mSkyViewProgram = 0;
	}
	if (mAerialPerspectiveProgram != 0)
	{
		glDeleteProgram(mAerialPerspectiveProgram);
		mAerialPerspectiveProgram = 0;
	}
	if (mMultiScatteringProgram != 0)
	{
		glDeleteProgram(mMultiScatteringProgram);
		mMultiScatteringProgram = 0;
	}
	if (mTransmittanceProgram != 0)
	{
		glDeleteProgram(mTransmittanceProgram);
		mTransmittanceProgram = 0;
	}
}

void SkyRendererGl::destroyAutoExposureResources()
{
	if (mAutoExposureMeterTex != 0)
	{
		glDeleteTextures(1, &mAutoExposureMeterTex);
		mAutoExposureMeterTex = 0;
	}
	if (mAutoExposureHistogramSsbo != 0)
	{
		glDeleteBuffers(1, &mAutoExposureHistogramSsbo);
		mAutoExposureHistogramSsbo = 0;
	}
}

void SkyRendererGl::destroyTransmittanceResources()
{
	if (mTransmittanceFbo != 0)
	{
		glDeleteFramebuffers(1, &mTransmittanceFbo);
		mTransmittanceFbo = 0;
	}
	if (mTransmittanceTex != 0)
	{
		glDeleteTextures(1, &mTransmittanceTex);
		mTransmittanceTex = 0;
	}
}

void SkyRendererGl::destroyMultipleScatteringResources()
{
	if (mMultiScatteringPreviewTex != 0)
	{
		glDeleteTextures(1, &mMultiScatteringPreviewTex);
		mMultiScatteringPreviewTex = 0;
	}
	if (mMultiScatteringTex != 0)
	{
		glDeleteTextures(1, &mMultiScatteringTex);
		mMultiScatteringTex = 0;
	}
}

void SkyRendererGl::destroySkyViewResources()
{
	if (mSkyViewFbo != 0)
	{
		glDeleteFramebuffers(1, &mSkyViewFbo);
		mSkyViewFbo = 0;
	}
	if (mSkyViewTex != 0)
	{
		glDeleteTextures(1, &mSkyViewTex);
		mSkyViewTex = 0;
	}
}

void SkyRendererGl::destroyAerialPerspectiveResources()
{
	if (mAerialPerspectivePreviewTex != 0)
	{
		glDeleteTextures(1, &mAerialPerspectivePreviewTex);
		mAerialPerspectivePreviewTex = 0;
	}
	if (mAerialPerspectiveTex != 0)
	{
		glDeleteTextures(1, &mAerialPerspectiveTex);
		mAerialPerspectiveTex = 0;
	}
}

void SkyRendererGl::destroyShadowResources()
{
	if (mShadowFallbackTex != 0)
	{
		glDeleteTextures(1, &mShadowFallbackTex);
		mShadowFallbackTex = 0;
	}
}

void SkyRendererGl::destroySceneResources()
{
	if (mFinalHdrFbo != 0)
	{
		glDeleteFramebuffers(1, &mFinalHdrFbo);
		mFinalHdrFbo = 0;
	}
	if (mFinalHdrTex != 0)
	{
		glDeleteTextures(1, &mFinalHdrTex);
		mFinalHdrTex = 0;
	}
	mFinalHdrMipLevel = 0;
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

bool SkyRendererGl::initialise()
{
	setupEarthAtmosphere(mAtmosphereInfo);
	updateViewAndSunDirections();
	createGpuPassTimers();
	glGenVertexArrays(1, &mFullscreenVao);

	if (!createPrograms())
	{
		shutdown();
		return false;
	}
	if (!createAutoExposureResources() || !createTransmittanceResources() || !createMultipleScatteringResources() || !createSkyViewResources() || !createAerialPerspectiveResources() || !createShadowResources() || !createSceneResources())
	{
		shutdown();
		return false;
	}

	mLutDirty = true;
	mSkyViewDirty = true;
	mAerialPerspectiveDirty = true;
	mInitialised = true;
	return true;
}

void SkyRendererGl::shutdown()
{
	clearExternalSceneTextures();
	clearExternalShadowMapTexture();
	destroyGpuPassTimers();
	destroySceneResources();
	destroyAutoExposureResources();
	destroyShadowResources();
	destroyAerialPerspectiveResources();
	destroySkyViewResources();
	destroyMultipleScatteringResources();
	destroyTransmittanceResources();
	destroyPrograms();
	if (mFullscreenVao != 0)
	{
		glDeleteVertexArrays(1, &mFullscreenVao);
		mFullscreenVao = 0;
	}
	mInitialised = false;
}

void SkyRendererGl::resize(int width, int height)
{
	const int newWidth = width > 1 ? width : 1;
	const int newHeight = height > 1 ? height : 1;
	if (newWidth != mBackbufferWidth || newHeight != mBackbufferHeight)
	{
		mBackbufferWidth = newWidth;
		mBackbufferHeight = newHeight;
		destroySceneResources();
		if (!createSceneResources())
		{
			mInitialised = false;
			return;
		}
		mAerialPerspectiveDirty = true;
	}
}

void SkyRendererGl::uploadAtmosphereUniforms(unsigned int program)
{
	const GlVec3 atmosphereCameraOffset = mCameraOffset;
	const float atmosphereCameraHeight = mCameraOffset.z;

	const auto set1f = [&](const char* name, float value)
	{
		const int loc = glGetUniformLocation(program, name);
		if (loc >= 0) glUniform1f(loc, value);
	};
	const auto set1i = [&](const char* name, int value)
	{
		const int loc = glGetUniformLocation(program, name);
		if (loc >= 0) glUniform1i(loc, value);
	};
	const auto set3f = [&](const char* name, const GlVec3& v)
	{
		const int loc = glGetUniformLocation(program, name);
		if (loc >= 0) glUniform3f(loc, v.x, v.y, v.z);
	};

	set1f("u_bottom_radius", mAtmosphereInfo.bottom_radius);
	set1f("u_top_radius", mAtmosphereInfo.top_radius);
	set1f("u_mie_phase_g", mAtmosphereInfo.mie_phase_g);
	set1f("u_rayleigh_density_exp_scale", mAtmosphereInfo.rayleigh_density_exp_scale);
	set1f("u_mie_density_exp_scale", mAtmosphereInfo.mie_density_exp_scale);
	set1f("u_absorption_layer0_width", mAtmosphereInfo.absorption_layer0_width);
	set1f("u_absorption_layer0_linear_term", mAtmosphereInfo.absorption_layer0_linear_term);
	set1f("u_absorption_layer0_constant_term", mAtmosphereInfo.absorption_layer0_constant_term);
	set1f("u_absorption_layer1_linear_term", mAtmosphereInfo.absorption_layer1_linear_term);
	set1f("u_absorption_layer1_constant_term", mAtmosphereInfo.absorption_layer1_constant_term);
	set1f("u_camera_height", atmosphereCameraHeight);
	set1f("u_raymarch_spp_min", static_cast<float>(mRayMarchMinSpp));
	set1f("u_raymarch_spp_max", static_cast<float>(mRayMarchMaxSpp));
	set1f("u_sun_illuminance", mSunIlluminanceScale);
	set1f("u_multiple_scattering_factor", mMultipleScatteringFactor);
	set3f("u_rayleigh_scattering", mAtmosphereInfo.rayleigh_scattering);
	set3f("u_mie_scattering", mAtmosphereInfo.mie_scattering);
	set3f("u_mie_extinction", mAtmosphereInfo.mie_extinction);
	set3f("u_mie_absorption", mAtmosphereInfo.mie_absorption);
	set3f("u_absorption_extinction", mAtmosphereInfo.absorption_extinction);
	set3f("u_ground_albedo", mAtmosphereInfo.ground_albedo);
	set3f("u_sun_direction", mSunDir);
	set3f("u_camera_offset", atmosphereCameraOffset);
	set3f("u_view_forward", mViewDir);
	set3f("u_view_right", mViewRight);
	set3f("u_view_up", mViewUp);

	set1i("u_transmittance_width", static_cast<int>(mLutsInfo.TRANSMITTANCE_TEXTURE_WIDTH));
	set1i("u_transmittance_height", static_cast<int>(mLutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT));
	set1i("u_multi_scattering_lut_res", static_cast<int>(mLutsInfo.MULTI_SCATTERING_TEXTURE_SIZE));
	set1i("u_skyview_width", static_cast<int>(mLutsInfo.SKY_VIEW_TEXTURE_WIDTH));
	set1i("u_skyview_height", static_cast<int>(mLutsInfo.SKY_VIEW_TEXTURE_HEIGHT));
	set1i("u_ap_width", static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_WIDTH));
	set1i("u_ap_height", static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_HEIGHT));
	set1i("u_ap_depth", static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH));
	set1i("u_fast_sky", mFastSky ? 1 : 0);
	set1i("u_fast_aerial_perspective", mFastAerialPerspective ? 1 : 0);
	set1i("u_colored_transmittance", (!mFastAerialPerspective && mColoredTransmittance) ? 1 : 0);
}

void SkyRendererGl::renderTransmittanceLut()
{
	beginGpuPassTimer(mTransmittancePassTimer);
	glBindFramebuffer(GL_FRAMEBUFFER, mTransmittanceFbo);
	glViewport(0, 0, static_cast<GLsizei>(mLutsInfo.TRANSMITTANCE_TEXTURE_WIDTH), static_cast<GLsizei>(mLutsInfo.TRANSMITTANCE_TEXTURE_HEIGHT));
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	glUseProgram(mTransmittanceProgram);
	uploadAtmosphereUniforms(mTransmittanceProgram);
	glBindVertexArray(mFullscreenVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	endGpuPassTimer(mTransmittancePassTimer);
}

void SkyRendererGl::renderMultipleScatteringLut()
{
	beginGpuPassTimer(mMultiScatteringPassTimer);
	glUseProgram(mMultiScatteringProgram);
	uploadAtmosphereUniforms(mMultiScatteringProgram);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTransmittanceTex);
	const int transLoc = glGetUniformLocation(mMultiScatteringProgram, "u_transmittance_lut");
	if (transLoc >= 0) glUniform1i(transLoc, 0);

	glBindImageTexture(0, mMultiScatteringTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glDispatchCompute(
		static_cast<GLuint>(mLutsInfo.MULTI_SCATTERING_TEXTURE_SIZE),
		static_cast<GLuint>(mLutsInfo.MULTI_SCATTERING_TEXTURE_SIZE),
		1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
	const unsigned int computeError = glGetError();
	if (computeError != GL_NO_ERROR)
	{
		char errorBuffer[128] = {};
		std::snprintf(errorBuffer, sizeof(errorBuffer), "glDispatchCompute failed with GL error 0x%X.", computeError);
		showMessageBox("OpenGL compute error", errorBuffer);
	}

	glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	endGpuPassTimer(mMultiScatteringPassTimer);

	updateMultiScatteringDebugStats();
}

void SkyRendererGl::updateMultiScatteringDebugStats()
{
	const size_t size = static_cast<size_t>(mLutsInfo.MULTI_SCATTERING_TEXTURE_SIZE);
	const size_t pixelCount = size * size;
	std::vector<float> pixels(pixelCount * 4, 0.0f);

	glBindTexture(GL_TEXTURE_2D, mMultiScatteringTex);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
	glBindTexture(GL_TEXTURE_2D, 0);

	const unsigned int err = glGetError();
	if (err != GL_NO_ERROR)
	{
		mMultiScatteringStatsValid = false;
		return;
	}

	float minV = std::numeric_limits<float>::infinity();
	float maxV = -std::numeric_limits<float>::infinity();
	for (size_t i = 0; i < pixelCount; ++i)
	{
		const size_t base = i * 4;
		const float r = pixels[base + 0];
		const float g = pixels[base + 1];
		const float b = pixels[base + 2];
		minV = (r < minV) ? r : minV;
		minV = (g < minV) ? g : minV;
		minV = (b < minV) ? b : minV;
		maxV = (r > maxV) ? r : maxV;
		maxV = (g > maxV) ? g : maxV;
		maxV = (b > maxV) ? b : maxV;
	}

	mMultiScatteringDebugMin = minV;
	mMultiScatteringDebugMax = maxV;
	mMultiScatteringStatsValid = true;
}

void SkyRendererGl::renderAerialPerspectiveVolume()
{
	beginGpuPassTimer(mAerialPerspectivePassTimer);
	glUseProgram(mAerialPerspectiveProgram);
	uploadAtmosphereUniforms(mAerialPerspectiveProgram);
	const int aspectLoc = glGetUniformLocation(mAerialPerspectiveProgram, "u_aspect");
	if (aspectLoc >= 0) glUniform1f(aspectLoc, static_cast<float>(mBackbufferWidth) / static_cast<float>(mBackbufferHeight));
	const int fovLoc = glGetUniformLocation(mAerialPerspectiveProgram, "u_fov_y_degrees");
	if (fovLoc >= 0) glUniform1f(fovLoc, kMainCameraFovYDegrees);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTransmittanceTex);
	const int transLoc = glGetUniformLocation(mAerialPerspectiveProgram, "u_transmittance_lut");
	if (transLoc >= 0) glUniform1i(transLoc, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mMultiScatteringTex);
	const int multiLoc = glGetUniformLocation(mAerialPerspectiveProgram, "u_multiscattering_lut");
	if (multiLoc >= 0) glUniform1i(multiLoc, 1);
	glActiveTexture(GL_TEXTURE2);
	const unsigned int shadowTex = (mShadowMapsEnabled && mHasExternalShadowMapTexture) ? mExternalShadowMapDepthTex : mShadowFallbackTex;
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	const int shadowTexLoc = glGetUniformLocation(mAerialPerspectiveProgram, "u_shadowmap_tex");
	if (shadowTexLoc >= 0) glUniform1i(shadowTexLoc, 2);
	const int shadowViewProjLoc = glGetUniformLocation(mAerialPerspectiveProgram, "u_shadow_view_proj");
	if (shadowViewProjLoc >= 0) glUniformMatrix4fv(shadowViewProjLoc, 1, GL_FALSE, mShadowViewProj);

	glBindImageTexture(0, mAerialPerspectiveTex, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	const GLuint groupX = (mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_WIDTH + 7u) / 8u;
	const GLuint groupY = (mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_HEIGHT + 7u) / 8u;
	const GLuint groupZ = mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH;
	glDispatchCompute(groupX, groupY, groupZ);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

	const unsigned int computeError = glGetError();
	if (computeError != GL_NO_ERROR)
	{
		char errorBuffer[128] = {};
		std::snprintf(errorBuffer, sizeof(errorBuffer), "Aerial perspective compute failed with GL error 0x%X.", computeError);
		showMessageBox("OpenGL compute error", errorBuffer);
	}

	glBindImageTexture(0, 0, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	endGpuPassTimer(mAerialPerspectivePassTimer);

	copyAerialPerspectivePreviewSlice();
	updateAerialPerspectiveDebugStats();
}

void SkyRendererGl::copyAerialPerspectivePreviewSlice()
{
	if (mAerialPerspectiveTex == 0 || mAerialPerspectivePreviewTex == 0)
	{
		return;
	}

	const int depth = static_cast<int>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH);
	if (depth <= 0)
	{
		return;
	}
	int slice = mAerialPerspectivePreviewSlice;
	if (slice < 0) slice = 0;
	if (slice >= depth) slice = depth - 1;
	mAerialPerspectivePreviewSlice = slice;

	glCopyImageSubData(
		mAerialPerspectiveTex,
		GL_TEXTURE_3D,
		0,
		0,
		0,
		slice,
		mAerialPerspectivePreviewTex,
		GL_TEXTURE_2D,
		0,
		0,
		0,
		0,
		static_cast<GLsizei>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_WIDTH),
		static_cast<GLsizei>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_HEIGHT),
		1);
	glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
}

void SkyRendererGl::updateLutPreviewTextures()
{
	if (mMultiScatteringTex != 0 && mMultiScatteringPreviewTex != 0)
	{
		const size_t size = static_cast<size_t>(mLutsInfo.MULTI_SCATTERING_TEXTURE_SIZE);
		std::vector<float> pixels(size * size * 4u, 0.0f);
		glBindTexture(GL_TEXTURE_2D, mMultiScatteringTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		const unsigned int readErr = glGetError();
		if (readErr == GL_NO_ERROR)
		{
			for (size_t i = 0; i < size * size; ++i)
			{
				const size_t base = i * 4u;
				pixels[base + 0] = toneMapPreviewValue(pixels[base + 0], mMultiScatteringPreviewExposure);
				pixels[base + 1] = toneMapPreviewValue(pixels[base + 1], mMultiScatteringPreviewExposure);
				pixels[base + 2] = toneMapPreviewValue(pixels[base + 2], mMultiScatteringPreviewExposure);
				pixels[base + 3] = 1.0f;
			}
			glBindTexture(GL_TEXTURE_2D, mMultiScatteringPreviewTex);
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				static_cast<GLsizei>(size),
				static_cast<GLsizei>(size),
				GL_RGBA,
				GL_FLOAT,
				pixels.data());
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}

	if (mAerialPerspectivePreviewTex != 0)
	{
		const size_t width = static_cast<size_t>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_WIDTH);
		const size_t height = static_cast<size_t>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_HEIGHT);
		std::vector<float> pixels(width * height * 4u, 0.0f);
		glBindTexture(GL_TEXTURE_2D, mAerialPerspectivePreviewTex);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
		const unsigned int readErr = glGetError();
		if (readErr == GL_NO_ERROR)
		{
			for (size_t i = 0; i < width * height; ++i)
			{
				const size_t base = i * 4u;
				pixels[base + 0] = toneMapPreviewValue(pixels[base + 0], mAerialPerspectivePreviewExposure);
				pixels[base + 1] = toneMapPreviewValue(pixels[base + 1], mAerialPerspectivePreviewExposure);
				pixels[base + 2] = toneMapPreviewValue(pixels[base + 2], mAerialPerspectivePreviewExposure);
				pixels[base + 3] = 1.0f;
			}
			glTexSubImage2D(
				GL_TEXTURE_2D,
				0,
				0,
				0,
				static_cast<GLsizei>(width),
				static_cast<GLsizei>(height),
				GL_RGBA,
				GL_FLOAT,
				pixels.data());
		}
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}

void SkyRendererGl::updateAerialPerspectiveDebugStats()
{
	const size_t width = static_cast<size_t>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_WIDTH);
	const size_t height = static_cast<size_t>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_HEIGHT);
	const size_t depth = static_cast<size_t>(mLutsInfo.AERIAL_PERSPECTIVE_TEXTURE_DEPTH);
	const size_t pixelCount = width * height * depth;
	std::vector<float> pixels(pixelCount * 4, 0.0f);

	glBindTexture(GL_TEXTURE_3D, mAerialPerspectiveTex);
	glGetTexImage(GL_TEXTURE_3D, 0, GL_RGBA, GL_FLOAT, pixels.data());
	glBindTexture(GL_TEXTURE_3D, 0);

	const unsigned int err = glGetError();
	if (err != GL_NO_ERROR)
	{
		mAerialPerspectiveStatsValid = false;
		return;
	}

	float minV = std::numeric_limits<float>::infinity();
	float maxV = -std::numeric_limits<float>::infinity();
	for (size_t i = 0; i < pixelCount; ++i)
	{
		const size_t base = i * 4;
		const float r = pixels[base + 0];
		const float g = pixels[base + 1];
		const float b = pixels[base + 2];
		minV = (r < minV) ? r : minV;
		minV = (g < minV) ? g : minV;
		minV = (b < minV) ? b : minV;
		maxV = (r > maxV) ? r : maxV;
		maxV = (g > maxV) ? g : maxV;
		maxV = (b > maxV) ? b : maxV;
	}

	mAerialPerspectiveDebugMin = minV;
	mAerialPerspectiveDebugMax = maxV;
	mAerialPerspectiveStatsValid = true;
}

void SkyRendererGl::renderSkyViewLut()
{
	beginGpuPassTimer(mSkyViewPassTimer);
	glBindFramebuffer(GL_FRAMEBUFFER, mSkyViewFbo);
	glViewport(0, 0, static_cast<GLsizei>(mLutsInfo.SKY_VIEW_TEXTURE_WIDTH), static_cast<GLsizei>(mLutsInfo.SKY_VIEW_TEXTURE_HEIGHT));
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	glUseProgram(mSkyViewProgram);
	uploadAtmosphereUniforms(mSkyViewProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTransmittanceTex);
	const int transLoc = glGetUniformLocation(mSkyViewProgram, "u_transmittance_lut");
	if (transLoc >= 0) glUniform1i(transLoc, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mMultiScatteringTex);
	const int multiLoc = glGetUniformLocation(mSkyViewProgram, "u_multiscattering_lut");
	if (multiLoc >= 0) glUniform1i(multiLoc, 1);
	glBindVertexArray(mFullscreenVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	endGpuPassTimer(mSkyViewPassTimer);
}

void SkyRendererGl::runAutoExposureHistogram()
{
	if (mAutoExposureHistogramProgram == 0 || mAutoExposureReduceProgram == 0 || mAutoExposureHistogramSsbo == 0 || mAutoExposureMeterTex == 0 || mFinalHdrTex == 0)
	{
		return;
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mAutoExposureHistogramSsbo);
	unsigned int clearValue = 0u;
	glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &clearValue);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glUseProgram(mAutoExposureHistogramProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mFinalHdrTex);
	const int hdrLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_hdr_tex");
	if (hdrLoc >= 0) glUniform1i(hdrLoc, 0);
	const int widthLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_width");
	if (widthLoc >= 0) glUniform1i(widthLoc, mBackbufferWidth);
	const int heightLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_height");
	if (heightLoc >= 0) glUniform1i(heightLoc, mBackbufferHeight);
	const int binCountLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_bin_count");
	if (binCountLoc >= 0) glUniform1i(binCountLoc, kAutoExposureHistogramBinCount);
	const int logMinLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_log_luminance_min");
	if (logMinLoc >= 0) glUniform1f(logMinLoc, -12.0f);
	const int logMaxLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_log_luminance_max");
	if (logMaxLoc >= 0) glUniform1f(logMaxLoc, 20.0f);
	const int maxLumLoc = glGetUniformLocation(mAutoExposureHistogramProgram, "u_max_sample_luminance");
	if (maxLumLoc >= 0) glUniform1f(maxLumLoc, 20000.0f);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mAutoExposureHistogramSsbo);
	const GLuint groupX = static_cast<GLuint>((mBackbufferWidth + 15) / 16);
	const GLuint groupY = static_cast<GLuint>((mBackbufferHeight + 15) / 16);
	glDispatchCompute(groupX > 0 ? groupX : 1, groupY > 0 ? groupY : 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	glUseProgram(mAutoExposureReduceProgram);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mAutoExposureHistogramSsbo);
	glBindImageTexture(0, mAutoExposureMeterTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
	const int reduceBinCountLoc = glGetUniformLocation(mAutoExposureReduceProgram, "u_bin_count");
	if (reduceBinCountLoc >= 0) glUniform1i(reduceBinCountLoc, kAutoExposureHistogramBinCount);
	const int reduceLogMinLoc = glGetUniformLocation(mAutoExposureReduceProgram, "u_log_luminance_min");
	if (reduceLogMinLoc >= 0) glUniform1f(reduceLogMinLoc, -12.0f);
	const int reduceLogMaxLoc = glGetUniformLocation(mAutoExposureReduceProgram, "u_log_luminance_max");
	if (reduceLogMaxLoc >= 0) glUniform1f(reduceLogMaxLoc, 20.0f);
	const float lowPercent = mAutoExposureHistogramLowPercent < 0.0f ? 0.0f : (mAutoExposureHistogramLowPercent > 100.0f ? 1.0f : (mAutoExposureHistogramLowPercent * 0.01f));
	float highPercent = mAutoExposureHistogramHighPercent < 0.0f ? 0.0f : (mAutoExposureHistogramHighPercent > 100.0f ? 1.0f : (mAutoExposureHistogramHighPercent * 0.01f));
	if (highPercent <= lowPercent)
	{
		highPercent = lowPercent + 0.01f;
	}
	if (highPercent > 1.0f)
	{
		highPercent = 1.0f;
	}
	const int lowPercentLoc = glGetUniformLocation(mAutoExposureReduceProgram, "u_low_percent");
	if (lowPercentLoc >= 0) glUniform1f(lowPercentLoc, lowPercent);
	const int highPercentLoc = glGetUniformLocation(mAutoExposureReduceProgram, "u_high_percent");
	if (highPercentLoc >= 0) glUniform1f(highPercentLoc, highPercent);
	const int fallbackLumLoc = glGetUniformLocation(mAutoExposureReduceProgram, "u_fallback_luminance");
	if (fallbackLumLoc >= 0) glUniform1f(fallbackLumLoc, 0.18f);
	glDispatchCompute(1, 1, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

	glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
}

void SkyRendererGl::renderPresent()
{
	beginGpuPassTimer(mPresentPassTimer);
	glBindFramebuffer(GL_FRAMEBUFFER, mFinalHdrFbo);
	glViewport(0, 0, mBackbufferWidth, mBackbufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	const float clearHdr[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	glClearBufferfv(GL_COLOR, 0, clearHdr);

	glUseProgram(mRaymarchProgram);
	uploadAtmosphereUniforms(mRaymarchProgram);
	const int aspectLoc = glGetUniformLocation(mRaymarchProgram, "u_aspect");
	if (aspectLoc >= 0) glUniform1f(aspectLoc, static_cast<float>(mBackbufferWidth) / static_cast<float>(mBackbufferHeight));
	const int fovLoc = glGetUniformLocation(mRaymarchProgram, "u_fov_y_degrees");
	if (fovLoc >= 0) glUniform1f(fovLoc, kMainCameraFovYDegrees);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTransmittanceTex);
	const int transLoc = glGetUniformLocation(mRaymarchProgram, "u_transmittance_lut");
	if (transLoc >= 0) glUniform1i(transLoc, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mSkyViewTex);
	const int skyLoc = glGetUniformLocation(mRaymarchProgram, "u_skyview_lut");
	if (skyLoc >= 0) glUniform1i(skyLoc, 1);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, mMultiScatteringTex);
	const int multiLoc = glGetUniformLocation(mRaymarchProgram, "u_multiscattering_lut");
	if (multiLoc >= 0) glUniform1i(multiLoc, 2);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_3D, mAerialPerspectiveTex);
	const int apLoc = glGetUniformLocation(mRaymarchProgram, "u_aerial_perspective_volume");
	if (apLoc >= 0) glUniform1i(apLoc, 3);
	glActiveTexture(GL_TEXTURE4);
	const unsigned int sceneColorTex = mHasExternalSceneTextures ? mExternalSceneHdrTex : mSceneHdrTex;
	glBindTexture(GL_TEXTURE_2D, sceneColorTex);
	const int sceneLoc = glGetUniformLocation(mRaymarchProgram, "u_scene_color");
	if (sceneLoc >= 0) glUniform1i(sceneLoc, 4);
	glActiveTexture(GL_TEXTURE5);
	const unsigned int sceneLinearDepthTex = mHasExternalSceneTextures ? mExternalSceneLinearDepthTex : mSceneLinearDepthTex;
	glBindTexture(GL_TEXTURE_2D, sceneLinearDepthTex);
	const int linearDepthLoc = glGetUniformLocation(mRaymarchProgram, "u_scene_linear_depth");
	if (linearDepthLoc >= 0) glUniform1i(linearDepthLoc, 5);
	glActiveTexture(GL_TEXTURE6);
	const unsigned int shadowTex = (mShadowMapsEnabled && mHasExternalShadowMapTexture) ? mExternalShadowMapDepthTex : mShadowFallbackTex;
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	const int shadowTexLoc = glGetUniformLocation(mRaymarchProgram, "u_shadowmap_tex");
	if (shadowTexLoc >= 0) glUniform1i(shadowTexLoc, 6);
	const int shadowViewProjLoc = glGetUniformLocation(mRaymarchProgram, "u_shadow_view_proj");
	if (shadowViewProjLoc >= 0) glUniformMatrix4fv(shadowViewProjLoc, 1, GL_FALSE, mShadowViewProj);
	const int fastSkyLoc = glGetUniformLocation(mRaymarchProgram, "u_fast_sky");
	if (fastSkyLoc >= 0) glUniform1i(fastSkyLoc, mFastSky ? 1 : 0);
	const int fastApLoc = glGetUniformLocation(mRaymarchProgram, "u_fast_aerial_perspective");
	if (fastApLoc >= 0) glUniform1i(fastApLoc, mFastAerialPerspective ? 1 : 0);
	const int apDepthLoc = glGetUniformLocation(mRaymarchProgram, "u_ap_debug_depth_km");
	if (apDepthLoc >= 0) glUniform1f(apDepthLoc, mAerialPerspectiveDebugDepthKm);
	glBindVertexArray(mFullscreenVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE5);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE4);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_3D, 0);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, mBackbufferWidth, mBackbufferHeight);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glClearColor(0.02f, 0.03f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, mFinalHdrTex);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	const bool useHistogramMetering =
		mAutoExposureEnabled &&
		mUseHistogramAutoExposure &&
		!mPhysicalModeEnabled &&
		mAutoExposureHistogramProgram != 0 &&
		mAutoExposureReduceProgram != 0 &&
		mAutoExposureHistogramSsbo != 0 &&
		mAutoExposureMeterTex != 0;
	if (useHistogramMetering)
	{
		runAutoExposureHistogram();
	}

	glUseProgram(mPostProcessProgram);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mFinalHdrTex);
	const int hdrLoc = glGetUniformLocation(mPostProcessProgram, "u_hdr_tex");
	if (hdrLoc >= 0) glUniform1i(hdrLoc, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mAutoExposureMeterTex);
	const int histogramMeterLoc = glGetUniformLocation(mPostProcessProgram, "u_auto_exposure_meter_tex");
	if (histogramMeterLoc >= 0) glUniform1i(histogramMeterLoc, 1);
	const int useHistogramLoc = glGetUniformLocation(mPostProcessProgram, "u_use_histogram_auto_exposure");
	if (useHistogramLoc >= 0) glUniform1i(useHistogramLoc, useHistogramMetering ? 1 : 0);
	const int agxLoc = glGetUniformLocation(mPostProcessProgram, "u_use_agx");
	if (agxLoc >= 0) glUniform1i(agxLoc, mUseAgxTonemap ? 1 : 0);
	const int autoExposureLoc = glGetUniformLocation(mPostProcessProgram, "u_auto_exposure");
	if (autoExposureLoc >= 0) glUniform1i(autoExposureLoc, mAutoExposureEnabled ? 1 : 0);
	const int manualExposureLoc = glGetUniformLocation(mPostProcessProgram, "u_manual_exposure");
	if (manualExposureLoc >= 0) glUniform1f(manualExposureLoc, mManualExposure);
	float exposureBiasEv = mExposureBiasEv;
	if (mSunAngleExposureBiasEnabled)
	{
		const float sunDirZ = mSunDir.z < -1.0f ? -1.0f : (mSunDir.z > 1.0f ? 1.0f : mSunDir.z);
		const float sunElevationDegrees = std::asin(sunDirZ) * (180.0f / 3.14159265f);
		const float sunElevationLerp = sunElevationDegrees <= 0.0f ? 0.0f : (sunElevationDegrees >= 90.0f ? 1.0f : (sunElevationDegrees / 90.0f));
		const float sunAngleBiasEv = mSunAngleExposureBiasAtHorizonEv +
			(mSunAngleExposureBiasAtNoonEv - mSunAngleExposureBiasAtHorizonEv) * sunElevationLerp;
		exposureBiasEv += sunAngleBiasEv;
	}
	const int exposureBiasLoc = glGetUniformLocation(mPostProcessProgram, "u_exposure_bias_ev");
	if (exposureBiasLoc >= 0) glUniform1f(exposureBiasLoc, exposureBiasEv);
	const int gammaLoc = glGetUniformLocation(mPostProcessProgram, "u_gamma");
	if (gammaLoc >= 0) glUniform1f(gammaLoc, mDisplayGamma);
	const int autoExposureKeyLoc = glGetUniformLocation(mPostProcessProgram, "u_auto_exposure_key");
	if (autoExposureKeyLoc >= 0) glUniform1f(autoExposureKeyLoc, mAutoExposureKey);
	const int agxSaturationLoc = glGetUniformLocation(mPostProcessProgram, "u_agx_saturation");
	if (agxSaturationLoc >= 0) glUniform1f(agxSaturationLoc, mAgxSaturation);
	const int physicalModeLoc = glGetUniformLocation(mPostProcessProgram, "u_physical_mode");
	if (physicalModeLoc >= 0) glUniform1i(physicalModeLoc, mPhysicalModeEnabled ? 1 : 0);
	const int cameraEvLoc = glGetUniformLocation(mPostProcessProgram, "u_camera_ev100");
	if (cameraEvLoc >= 0) glUniform1f(cameraEvLoc, mCameraEv100);
	const int outputSrgbLoc = glGetUniformLocation(mPostProcessProgram, "u_output_srgb");
	if (outputSrgbLoc >= 0) glUniform1i(outputSrgbLoc, (mPhysicalModeEnabled || mOutputSrgb) ? 1 : 0);
	const int autoExposureMipLoc = glGetUniformLocation(mPostProcessProgram, "u_auto_exposure_mip_level");
	if (autoExposureMipLoc >= 0) glUniform1i(autoExposureMipLoc, mFinalHdrMipLevel);
	glBindVertexArray(mFullscreenVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glUseProgram(0);
	endGpuPassTimer(mPresentPassTimer);
}

void SkyRendererGl::render()
{
	if (!mInitialised)
	{
		return;
	}

	updateViewAndSunDirections();
	resolveGpuPassTimers();

	if (!mHasExternalSceneTextures)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mSceneFbo);
		glViewport(0, 0, mBackbufferWidth, mBackbufferHeight);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		const float clearLinearDepth[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		const float clearDepth = 1.0f;
		glClearBufferfv(GL_COLOR, 0, clearColor);
		glClearBufferfv(GL_COLOR, 1, clearLinearDepth);
		glClearBufferfv(GL_DEPTH, 0, &clearDepth);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	if (mLutDirty)
	{
		renderTransmittanceLut();
		renderMultipleScatteringLut();
		mLutDirty = false;
		mSkyViewDirty = true;
		mAerialPerspectiveDirty = true;
	}
	if (mSkyViewDirty)
	{
		renderSkyViewLut();
		mSkyViewDirty = false;
	}
	if (mAerialPerspectiveDirty)
	{
		renderAerialPerspectiveVolume();
		mAerialPerspectiveDirty = false;
	}
	copyAerialPerspectivePreviewSlice();
	updateLutPreviewTextures();
	renderPresent();
}
