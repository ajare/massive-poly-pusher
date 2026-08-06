#include "mpp/PbrMaterialFeatures.h"
#include "mpp/PbrMaterialTests.h"

namespace mpp
{
	bool runPbrMaterialSpecializationTests(std::string* failure)
	{
		auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
		PbrMaterialSpecification::PbrSurface surface;
		surface.metallicFactor = 0.0f;
		surface.roughnessFactor = 0.0f;
		surface.normalScale = 0.0f;
		surface.occlusionStrength = 0.0f;
		std::vector<PbrMaterialSpecification::TextureOptions> textures;
		auto minimal = derivePbrMaterialFeatures(surface, textures);
		if (minimal != 0 || describePbrMaterialFeatures(minimal) != "Minimal") return fail("minimal feature derivation failed");

		auto addMap = [&](char const* sampler)
		{
			PbrMaterialSpecification::TextureOptions texture;
			texture.resourceExists = true; texture.sampler = sampler; textures.push_back(texture);
		};
		addMap("PBR_BASE_COLOUR_MAP"); addMap("PBR_METALLIC_ROUGHNESS_MAP"); addMap("PBR_NORMAL_MAP");
		addMap("PBR_OCCLUSION_MAP"); addMap("PBR_EMISSIVE_MAP");
		surface.metallicFactor = 1.0f; surface.roughnessFactor = 0.5f; surface.normalScale = 1.0f;
		surface.occlusionStrength = 1.0f; surface.emissiveFactor = { 0.0f, 2.0f, 0.0f };
		surface.alphaMode = PbrMaterialSpecification::PbrAlphaMode::Mask; surface.doubleSided = true;
		auto full = derivePbrMaterialFeatures(surface, textures);
		for (auto feature : { PbrMaterialFeature::BaseColourMap, PbrMaterialFeature::Metallic, PbrMaterialFeature::Roughness,
			PbrMaterialFeature::MetallicRoughnessMap, PbrMaterialFeature::NormalMap, PbrMaterialFeature::Occlusion,
			PbrMaterialFeature::Emissive, PbrMaterialFeature::AlphaMask, PbrMaterialFeature::DoubleSided })
			if (!hasPbrFeature(full, feature)) return fail("full feature derivation omitted a required bit");
		if (hasPbrFeature(full, PbrMaterialFeature::AlphaBlend)) return fail("mask derivation also enabled blend");

		auto defines = makePbrSpecializationDefines(full);
		if (defines.find("#define PBR_SPEC_NORMAL_MAP 1") == std::string::npos || defines.find("#define PBR_SPEC_ALPHA_BLEND 0") == std::string::npos)
			return fail("specialization define generation failed");
		auto injected = injectPbrSpecializationDefines("@@Version\nvoid main() {}", full);
		if (injected.rfind("@@Version\n#define PBR_SPEC_", 0) != 0) return fail("specialization defines were not inserted after @@Version");
		if (!hasPbrFeature(derivePbrMaterialFeatures(surface, textures, true), PbrMaterialFeature::LegacyFullContract)) return fail("legacy contract marker was lost");
		return true;
	}
}
