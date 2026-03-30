// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace pbrsky
{
	class SkyAtmosphereRenderer;
}

struct GLFWwindow;

void runGlfwMainLoop(GLFWwindow* window, pbrsky::SkyAtmosphereRenderer& renderer);
