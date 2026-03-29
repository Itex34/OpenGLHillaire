#version 430 core

in vec2 v_uv;
layout(location = 0) out vec4 o_color;

uniform sampler2D u_hdr_tex;
uniform sampler2D u_auto_exposure_meter_tex;
uniform int u_use_agx;
uniform int u_auto_exposure;
uniform int u_use_histogram_auto_exposure;
uniform int u_auto_exposure_mip_level;
uniform float u_manual_exposure;
uniform float u_exposure_bias_ev;
uniform float u_gamma;
uniform float u_auto_exposure_key;
uniform float u_agx_saturation;
uniform int u_physical_mode;
uniform float u_camera_ev100;
uniform int u_output_srgb;

vec3 toneMapLegacy(vec3 hdr)
{
	const vec3 white_point = vec3(1.08241, 0.96756, 0.95003);
	return vec3(1.0) - exp(-max(hdr, vec3(0.0)) / white_point);
}

vec3 agxInsetMatrix(vec3 c)
{
	return vec3(
		dot(c, vec3(0.842479062253094, 0.0423282422610123, 0.0423756549057051)),
		dot(c, vec3(0.0784335999999992, 0.878468636469772, 0.0784336000000000)),
		dot(c, vec3(0.0792237451477643, 0.0791661274605434, 0.879142973793104))
	);
}

vec3 agxOutsetMatrix(vec3 c)
{
	return vec3(
		dot(c, vec3(1.196879005120170, -0.052896851757456, -0.052971635514444)),
		dot(c, vec3(-0.098020881140137, 1.151903129904170, -0.098043450117124)),
		dot(c, vec3(-0.099029744079721, -0.098961176844843, 1.151073672641160))
	);
}

vec3 agxContrastApprox(vec3 x)
{
	vec3 x2 = x * x;
	vec3 x4 = x2 * x2;
	return
		15.5 * x4 * x2
		- 40.14 * x4 * x
		+ 31.96 * x4
		- 6.868 * x2 * x
		+ 0.4298 * x2
		+ 0.1191 * x
		- 0.00232;
}

vec3 toneMapAgx(vec3 hdr)
{
	const float min_ev = -12.47393;
	const float max_ev = 4.026069;
	vec3 encoded = agxInsetMatrix(max(hdr, vec3(1e-10)));
	encoded = log2(encoded);
	encoded = clamp((encoded - min_ev) / (max_ev - min_ev), vec3(0.0), vec3(1.0));
	encoded = agxContrastApprox(encoded);
	encoded = agxOutsetMatrix(clamp(encoded, vec3(0.0), vec3(1.0)));
	encoded = pow(max(encoded, vec3(0.0)), vec3(2.2));
	const vec3 luma_weights = vec3(0.2126, 0.7152, 0.0722);
	float luma = dot(encoded, luma_weights);
	encoded = mix(vec3(luma), encoded, u_agx_saturation);
	return max(encoded, vec3(0.0));
}

float computeExposure()
{
	if (u_physical_mode != 0)
	{
		// EV100 -> scene-linear exposure scale.
		return 1.0 / (1.2 * exp2(u_camera_ev100));
	}

	float exposure = max(u_manual_exposure, 0.001);
	if (u_auto_exposure != 0)
	{
		if (u_use_histogram_auto_exposure != 0)
		{
			float meterLuminance = max(texture(u_auto_exposure_meter_tex, vec2(0.5, 0.5)).r, 1e-4);
			exposure = max(u_auto_exposure_key, 0.01) / meterLuminance;
		}
		else
		{
			vec3 avgHdr = textureLod(u_hdr_tex, vec2(0.5, 0.5), float(max(u_auto_exposure_mip_level, 0))).rgb;
			avgHdr = max(avgHdr, vec3(0.0));
			const vec3 luma = vec3(0.2126, 0.7152, 0.0722);
			float avgLuminance = max(dot(avgHdr, luma), 1e-4);
			exposure = max(u_auto_exposure_key, 0.01) / avgLuminance;
		}
	}
	exposure *= exp2(u_exposure_bias_ev);
	return max(exposure, 0.001);
}

vec3 linearToSrgb(vec3 linearColor)
{
	vec3 x = max(linearColor, vec3(0.0));
	bvec3 cutoff = lessThanEqual(x, vec3(0.0031308));
	vec3 lower = 12.92 * x;
	vec3 higher = 1.055 * pow(x, vec3(1.0 / 2.4)) - 0.055;
	return mix(higher, lower, vec3(cutoff));
}

void main()
{
	vec3 hdr = texture(u_hdr_tex, clamp(v_uv, vec2(0.0), vec2(1.0))).rgb;
	float exposure = computeExposure();
	vec3 exposed = max(hdr, vec3(0.0)) * exposure;
	vec3 displayLinear = exposed;
	if (u_physical_mode == 0)
	{
		displayLinear = (u_use_agx != 0) ? toneMapAgx(exposed) : toneMapLegacy(exposed);
	}

	vec3 color = max(displayLinear, vec3(0.0));
	if (u_output_srgb != 0 || u_physical_mode != 0)
	{
		color = linearToSrgb(color);
	}
	else
	{
		const float safeGamma = max(u_gamma, 0.01);
		color = pow(color, vec3(1.0 / safeGamma));
	}

	o_color = vec4(clamp(color, vec3(0.0), vec3(1.0)), 1.0);
}
