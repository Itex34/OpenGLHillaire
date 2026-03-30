// Copyright Epic Games, Inc. All Rights Reserved.

#include "GlfwAppLoop.h"

#include "GlfwAppState.h"
#include "GlfwAppUi.h"
#include "TerrainSceneRenderer.h"

#include "PbrSkyLibOpenGL/SkyAtmosphereRenderer.h"

#include <cmath>
#include <cstdio>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include "imgui/examples/imgui_impl_glfw.h"
#include "imgui/examples/imgui_impl_opengl3.h"

namespace
{
	float vecLength(const pbrsky::Vec3& v)
	{
		return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	void vecNormalize(const pbrsky::Vec3& v, float len, float out[3])
	{
		if (len > 1e-6f)
		{
			out[0] = v.x / len;
			out[1] = v.y / len;
			out[2] = v.z / len;
		}
		else
		{
			out[0] = 0.0f;
			out[1] = 0.0f;
			out[2] = 0.0f;
		}
	}

	pbrsky::Vec3 vecScale(const float c[3], float s)
	{
		return { c[0] * s, c[1] * s, c[2] * s };
	}

	void initialiseUiState(pbrsky::SkyAtmosphereRenderer& renderer, GlfwUiState& state)
	{
		state.uiCamHeight = renderer.getCameraHeight();
		state.uiCamForward = renderer.getCameraForward();
		state.uiIllumScale = renderer.getSunIlluminanceScale();
		state.uiSunYaw = renderer.getSunYaw();
		state.uiSunPitch = renderer.getSunPitch();
		state.uiMinSpp = renderer.getRayMarchMinSpp();
		state.uiMaxSpp = renderer.getRayMarchMaxSpp();
		state.uiFastSky = renderer.getFastSky();
		state.uiFastAerialPerspective = renderer.getFastAerialPerspective();
		state.uiShadowMaps = renderer.getShadowMapsEnabled();
		state.uiColoredTransmittance = renderer.getColoredTransmittance();
		state.uiMultiScattering = renderer.getMultipleScatteringFactor();
		state.uiUseAgxTonemap = renderer.getUseAgxTonemap();
		state.uiAutoExposure = renderer.getAutoExposureEnabled();
		state.uiManualExposure = renderer.getManualExposure();
		state.uiExposureBiasEv = renderer.getExposureBiasEv();
		state.uiUseHistogramAutoExposure = renderer.getUseHistogramAutoExposure();
		state.uiAutoExposureHistogramLowPercent = renderer.getAutoExposureHistogramLowPercent();
		state.uiAutoExposureHistogramHighPercent = renderer.getAutoExposureHistogramHighPercent();
		state.uiSunAngleExposureBias = renderer.getSunAngleExposureBiasEnabled();
		state.uiSunAngleExposureBiasAtHorizonEv = renderer.getSunAngleExposureBiasAtHorizonEv();
		state.uiSunAngleExposureBiasAtNoonEv = renderer.getSunAngleExposureBiasAtNoonEv();
		state.uiDisplayGamma = renderer.getDisplayGamma();
		state.uiAutoExposureKey = renderer.getAutoExposureKey();
		state.uiAgxSaturation = renderer.getAgxSaturation();
		state.uiPhysicalMode = renderer.getPhysicalModeEnabled();
		state.uiCameraEv100 = renderer.getCameraEv100();
		state.uiOutputSrgb = renderer.getOutputSrgb();
		state.apPreviewSlice = renderer.getAerialPerspectivePreviewSlice();
		state.uiViewYaw = renderer.getViewYaw();
		state.uiViewPitch = renderer.getViewPitch();

		const pbrsky::AtmosphereInfo atmosphere = renderer.getAtmosphereInfo();
		state.uiMiePhase = atmosphere.mie_phase_g;
		state.uiMieScattScale = vecLength(atmosphere.mie_scattering);
		vecNormalize(atmosphere.mie_scattering, state.uiMieScattScale, state.uiMieScattColor);
		state.uiMieAbsScale = vecLength(atmosphere.mie_absorption);
		vecNormalize(atmosphere.mie_absorption, state.uiMieAbsScale, state.uiMieAbsColor);
		state.uiRayScattScale = vecLength(atmosphere.rayleigh_scattering);
		vecNormalize(atmosphere.rayleigh_scattering, state.uiRayScattScale, state.uiRayScattColor);
		state.uiAbsorpScale = vecLength(atmosphere.absorption_extinction);
		vecNormalize(atmosphere.absorption_extinction, state.uiAbsorpScale, state.uiAbsorpColor);
		state.uiBottomRadius = atmosphere.bottom_radius;
		state.uiAtmosphereHeight = atmosphere.top_radius - atmosphere.bottom_radius;
		state.uiMieScaleHeight = -1.0f / atmosphere.mie_density_exp_scale;
		state.uiRayScaleHeight = -1.0f / atmosphere.rayleigh_density_exp_scale;
		state.uiGroundAlbedo[0] = atmosphere.ground_albedo.x;
		state.uiGroundAlbedo[1] = atmosphere.ground_albedo.y;
		state.uiGroundAlbedo[2] = atmosphere.ground_albedo.z;
		state.uiPrevTimeSec = glfwGetTime();
	}

	void updateFrameInputs(GLFWwindow* window, pbrsky::SkyAtmosphereRenderer& renderer, GlfwUiState& state)
	{
		double nowSec = glfwGetTime();
		float dt = static_cast<float>(nowSec - state.uiPrevTimeSec);
		state.uiPrevTimeSec = nowSec;
		if (dt < 0.0f) dt = 0.0f;
		if (dt > 0.25f) dt = 0.25f;

		const bool f1Down = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
		if (f1Down && !state.f1WasDown)
		{
			state.uiPointerLock = !state.uiPointerLock;
		}
		state.f1WasDown = f1Down;
		if (!state.uiFpsCamera)
		{
			state.uiPointerLock = false;
		}
		if (state.uiPointerLock != state.pointerLockApplied)
		{
			glfwSetInputMode(window, GLFW_CURSOR, state.uiPointerLock ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
			state.pointerLockApplied = state.uiPointerLock;
			state.mouseDeltaInit = false;
		}

		int fbWidth = 0;
		int fbHeight = 0;
		glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
		renderer.resize(fbWidth, fbHeight);

		if (state.uiAnimateSun)
		{
			state.uiSunPitch += state.uiSunAnimSpeed * dt;
			if (state.uiSunPitch > 3.14159265f) state.uiSunPitch -= 6.2831853f;
			if (state.uiSunPitch < -3.14159265f) state.uiSunPitch += 6.2831853f;
		}

		renderer.setViewYaw(state.uiViewYaw);
		renderer.setViewPitch(state.uiViewPitch);

		if (state.uiFpsCamera)
		{
			if (state.uiPointerLock)
			{
				double mouseX = 0.0;
				double mouseY = 0.0;
				glfwGetCursorPos(window, &mouseX, &mouseY);
				if (!state.mouseDeltaInit)
				{
					state.lastMouseX = mouseX;
					state.lastMouseY = mouseY;
					state.mouseDeltaInit = true;
				}
				else
				{
					const double deltaX = mouseX - state.lastMouseX;
					const double deltaY = mouseY - state.lastMouseY;
					state.lastMouseX = mouseX;
					state.lastMouseY = mouseY;
					state.uiViewYaw -= static_cast<float>(deltaX) * state.uiMouseSensitivity;
					state.uiViewPitch -= static_cast<float>(deltaY) * state.uiMouseSensitivity;
					if (state.uiViewPitch > 1.55f) state.uiViewPitch = 1.55f;
					if (state.uiViewPitch < -1.55f) state.uiViewPitch = -1.55f;
					renderer.setViewYaw(state.uiViewYaw);
					renderer.setViewPitch(state.uiViewPitch);
				}
			}
			else
			{
				state.mouseDeltaInit = false;
			}

			pbrsky::Vec3 camera = renderer.getCameraOffset();
			const pbrsky::Vec3 forward = renderer.getViewDir();
			const pbrsky::Vec3 right = renderer.getViewRight();
			const pbrsky::Vec3 up = { 0.0f, 0.0f, 1.0f };
			pbrsky::Vec3 move = { 0.0f, 0.0f, 0.0f };
			if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { move.x += forward.x; move.y += forward.y; move.z += forward.z; }
			if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { move.x -= forward.x; move.y -= forward.y; move.z -= forward.z; }
			if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { move.x += right.x; move.y += right.y; move.z += right.z; }
			if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { move.x -= right.x; move.y -= right.y; move.z -= right.z; }
			if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) { move.x += up.x; move.y += up.y; move.z += up.z; }
			if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) { move.x -= up.x; move.y -= up.y; move.z -= up.z; }

			const float moveLen = std::sqrt(move.x * move.x + move.y * move.y + move.z * move.z);
			if (moveLen > 1e-6f)
			{
				move.x /= moveLen;
				move.y /= moveLen;
				move.z /= moveLen;
				float speed = state.uiCameraMoveSpeed;
				if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
				{
					speed *= 3.0f;
				}
				camera.x += move.x * speed * dt;
				camera.y += move.y * speed * dt;
				camera.z += move.z * speed * dt;
				renderer.setCameraOffset(camera);
			}

			const pbrsky::Vec3 cameraNow = renderer.getCameraOffset();
			state.uiCamForward = cameraNow.y;
			state.uiCamHeight = cameraNow.z;
		}
		else
		{
			renderer.setCameraHeight(state.uiCamHeight);
			renderer.setCameraForward(state.uiCamForward);
		}

		renderer.setSunIlluminanceScale(state.uiIllumScale);
		renderer.setSunYaw(state.uiSunYaw);
		renderer.setSunPitch(state.uiSunPitch);
		renderer.setRayMarchMinSpp(state.uiMinSpp);
		renderer.setRayMarchMaxSpp(state.uiMaxSpp);
		renderer.setFastSky(state.uiFastSky);
		renderer.setFastAerialPerspective(state.uiFastAerialPerspective);
		renderer.setShadowMapsEnabled(state.uiShadowMaps);
		renderer.setColoredTransmittance(state.uiColoredTransmittance && !state.uiFastAerialPerspective);
		renderer.setMultipleScatteringFactor(state.uiMultiScattering);
		renderer.setUseAgxTonemap(state.uiUseAgxTonemap);
		renderer.setAutoExposureEnabled(state.uiAutoExposure);
		renderer.setManualExposure(state.uiManualExposure);
		renderer.setExposureBiasEv(state.uiExposureBiasEv);
		renderer.setUseHistogramAutoExposure(state.uiUseHistogramAutoExposure);
		renderer.setAutoExposureHistogramLowPercent(state.uiAutoExposureHistogramLowPercent);
		renderer.setAutoExposureHistogramHighPercent(state.uiAutoExposureHistogramHighPercent);
		renderer.setSunAngleExposureBiasEnabled(state.uiSunAngleExposureBias);
		renderer.setSunAngleExposureBiasAtHorizonEv(state.uiSunAngleExposureBiasAtHorizonEv);
		renderer.setSunAngleExposureBiasAtNoonEv(state.uiSunAngleExposureBiasAtNoonEv);
		renderer.setDisplayGamma(state.uiDisplayGamma);
		renderer.setAutoExposureKey(state.uiAutoExposureKey);
		renderer.setAgxSaturation(state.uiAgxSaturation);
		renderer.setPhysicalModeEnabled(state.uiPhysicalMode);
		renderer.setCameraEv100(state.uiCameraEv100);
		renderer.setOutputSrgb(state.uiOutputSrgb);
		renderer.setAerialPerspectiveDebugDepthKm(state.apDebugDepthKm);
		renderer.setAerialPerspectivePreviewSlice(state.apPreviewSlice);
		renderer.setMultiScatteringPreviewExposure(state.msPreviewExposure);
		renderer.setAerialPerspectivePreviewExposure(state.apPreviewExposure);
		if (state.applyAtmosphereUi)
		{
			pbrsky::AtmosphereInfo atmosphere = renderer.getAtmosphereInfo();
			atmosphere.mie_phase_g = state.uiMiePhase;
			atmosphere.mie_scattering = vecScale(state.uiMieScattColor, state.uiMieScattScale);
			atmosphere.mie_absorption = vecScale(state.uiMieAbsColor, state.uiMieAbsScale);
			atmosphere.mie_extinction = {
				atmosphere.mie_scattering.x + atmosphere.mie_absorption.x,
				atmosphere.mie_scattering.y + atmosphere.mie_absorption.y,
				atmosphere.mie_scattering.z + atmosphere.mie_absorption.z
			};
			atmosphere.rayleigh_scattering = vecScale(state.uiRayScattColor, state.uiRayScattScale);
			atmosphere.absorption_extinction = vecScale(state.uiAbsorpColor, state.uiAbsorpScale);
			atmosphere.bottom_radius = state.uiBottomRadius;
			atmosphere.top_radius = state.uiBottomRadius + state.uiAtmosphereHeight;
			atmosphere.mie_density_exp_scale = -1.0f / (state.uiMieScaleHeight > 0.001f ? state.uiMieScaleHeight : 0.001f);
			atmosphere.rayleigh_density_exp_scale = -1.0f / (state.uiRayScaleHeight > 0.001f ? state.uiRayScaleHeight : 0.001f);
			atmosphere.ground_albedo = { state.uiGroundAlbedo[0], state.uiGroundAlbedo[1], state.uiGroundAlbedo[2] };
			renderer.setAtmosphereInfo(atmosphere);
			state.applyAtmosphereUi = false;
		}
		state.uiMinSpp = renderer.getRayMarchMinSpp();
		state.uiMaxSpp = renderer.getRayMarchMaxSpp();
		state.uiManualExposure = renderer.getManualExposure();
		state.uiExposureBiasEv = renderer.getExposureBiasEv();
		state.uiUseHistogramAutoExposure = renderer.getUseHistogramAutoExposure();
		state.uiAutoExposureHistogramLowPercent = renderer.getAutoExposureHistogramLowPercent();
		state.uiAutoExposureHistogramHighPercent = renderer.getAutoExposureHistogramHighPercent();
		state.uiSunAngleExposureBias = renderer.getSunAngleExposureBiasEnabled();
		state.uiSunAngleExposureBiasAtHorizonEv = renderer.getSunAngleExposureBiasAtHorizonEv();
		state.uiSunAngleExposureBiasAtNoonEv = renderer.getSunAngleExposureBiasAtNoonEv();
		state.uiDisplayGamma = renderer.getDisplayGamma();
		state.uiAutoExposureKey = renderer.getAutoExposureKey();
		state.uiAgxSaturation = renderer.getAgxSaturation();
		state.uiPhysicalMode = renderer.getPhysicalModeEnabled();
		state.uiCameraEv100 = renderer.getCameraEv100();
		state.uiOutputSrgb = renderer.getOutputSrgb();
	}
}

void runGlfwMainLoop(GLFWwindow* window, pbrsky::SkyAtmosphereRenderer& renderer)
{
	GlfwUiState state;
	initialiseUiState(renderer, state);

	TerrainSceneRenderer terrainRenderer;
	const bool terrainReady = terrainRenderer.initialise();
	if (!terrainReady)
	{
		std::fprintf(stderr, "TerrainSceneRenderer initialization failed. Running sky-only viewer.\n");
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GLFW_TRUE);
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		updateFrameInputs(window, renderer, state);

		if (terrainReady)
		{
			int fbWidth = 0;
			int fbHeight = 0;
			glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
			terrainRenderer.resize(fbWidth, fbHeight);
			terrainRenderer.render(renderer, state.uiRenderTerrain, state.uiShadowMaps);
			renderer.setExternalSceneTextures(terrainRenderer.getSceneColorTexture(), terrainRenderer.getSceneLinearDepthTexture());
			if (state.uiShadowMaps)
			{
				renderer.setExternalShadowMapTexture(terrainRenderer.getShadowDepthTexture());
				renderer.setExternalShadowViewProj(terrainRenderer.getShadowViewProjMatrix());
			}
			else
			{
				renderer.clearExternalShadowMapTexture();
			}
			state.uiTerrainPassMs = terrainRenderer.getTerrainPassMs();
		}
		else
		{
			renderer.clearExternalSceneTextures();
			renderer.clearExternalShadowMapTexture();
			state.uiTerrainPassMs = 0.0f;
		}

		renderer.render();
		drawGlfwUi(renderer, state);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(window);
	}

	if (terrainReady)
	{
		terrainRenderer.shutdown();
	}
}
