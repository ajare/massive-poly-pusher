#include <algorithm>
#include <array>
#include <sstream>

#include "mpp/MppException.h"
#include "mpp/PbrMaterialFeatures.h"

using namespace std;

namespace mpp
{
	namespace
	{
		bool hasMap(vector<PbrMaterialSpecification::TextureOptions> const& textures, char const* sampler)
		{
			return any_of(textures.begin(), textures.end(), [&](auto const& texture)
			{
				return texture.resourceExists && texture.sampler == sampler;
			});
		}
	}

	PbrMaterialFeatures derivePbrMaterialFeatures(PbrMaterialSpecification::PbrSurface const& surface,
		vector<PbrMaterialSpecification::TextureOptions> const& textures, bool legacyFullContract)
	{
		if (legacyFullContract) return pbrFeature(PbrMaterialFeature::LegacyFullContract);
		PbrMaterialFeatures features = 0;
		auto enable = [&](PbrMaterialFeature feature) { features |= pbrFeature(feature); };
		if (hasMap(textures, "PBR_BASE_COLOUR_MAP")) enable(PbrMaterialFeature::BaseColourMap);
		if (surface.metallicFactor > 0.0f || hasMap(textures, "PBR_METALLIC_MAP")) enable(PbrMaterialFeature::Metallic);
		if (surface.roughnessFactor > 0.0f || hasMap(textures, "PBR_ROUGHNESS_MAP")) enable(PbrMaterialFeature::Roughness);
		if (hasMap(textures, "PBR_METALLIC_MAP")) enable(PbrMaterialFeature::MetallicMap);
		if (hasMap(textures, "PBR_ROUGHNESS_MAP")) enable(PbrMaterialFeature::RoughnessMap);
		if (hasMap(textures, "PBR_METALLIC_ROUGHNESS_MAP") &&
			(hasPbrFeature(features, PbrMaterialFeature::Metallic) || hasPbrFeature(features, PbrMaterialFeature::Roughness)))
			enable(PbrMaterialFeature::MetallicRoughnessMap);
		if (hasMap(textures, "PBR_NORMAL_MAP") && surface.normalScale > 0.0f) enable(PbrMaterialFeature::NormalMap);
		if (hasMap(textures, "PBR_OCCLUSION_MAP") && surface.occlusionStrength > 0.0f) enable(PbrMaterialFeature::Occlusion);
		if (surface.emissiveFactor.r > 0.0f || surface.emissiveFactor.g > 0.0f || surface.emissiveFactor.b > 0.0f || hasMap(textures, "PBR_EMISSIVE_MAP")) enable(PbrMaterialFeature::Emissive);
		if (hasMap(textures, "PBR_EMISSIVE_MAP")) enable(PbrMaterialFeature::EmissiveMap);
		if (surface.alphaMode == PbrMaterialSpecification::PbrAlphaMode::Mask) enable(PbrMaterialFeature::AlphaMask);
		else if (surface.alphaMode == PbrMaterialSpecification::PbrAlphaMode::Blend) enable(PbrMaterialFeature::AlphaBlend);
		if (surface.doubleSided) enable(PbrMaterialFeature::DoubleSided);
		return features;
	}

	string describePbrMaterialFeatures(PbrMaterialFeatures features)
	{
		if (hasPbrFeature(features, PbrMaterialFeature::LegacyFullContract)) return "LegacyFullContract";
		array<pair<PbrMaterialFeature, char const*>, 13> const names = {{
			{ PbrMaterialFeature::BaseColourMap, "BaseColourMap" }, { PbrMaterialFeature::Metallic, "Metallic" },
			{ PbrMaterialFeature::Roughness, "Roughness" }, { PbrMaterialFeature::MetallicRoughnessMap, "MetallicRoughnessMap" },
			{ PbrMaterialFeature::MetallicMap, "MetallicMap" }, { PbrMaterialFeature::RoughnessMap, "RoughnessMap" }, { PbrMaterialFeature::NormalMap, "NormalMap" }, { PbrMaterialFeature::Occlusion, "Occlusion" },
			{ PbrMaterialFeature::Emissive, "Emissive" }, { PbrMaterialFeature::EmissiveMap, "EmissiveMap" }, { PbrMaterialFeature::AlphaMask, "AlphaMask" },
			{ PbrMaterialFeature::AlphaBlend, "AlphaBlend" }, { PbrMaterialFeature::DoubleSided, "DoubleSided" }
		}};
		ostringstream result;
		bool first = true;
		for (auto const& [feature, name] : names) if (hasPbrFeature(features, feature))
		{
			if (!first) result << '|';
			result << name; first = false;
		}
		return first ? "Minimal" : result.str();
	}

	string makePbrSpecializationDefines(PbrMaterialFeatures features)
	{
		auto value = [&](PbrMaterialFeature feature) { return hasPbrFeature(features, feature) ? 1 : 0; };
		bool const legacy = hasPbrFeature(features, PbrMaterialFeature::LegacyFullContract);
		ostringstream source;
		source << "#define PBR_SPEC_LEGACY_FULL_CONTRACT " << (legacy ? 1 : 0) << '\n'
			<< "#define PBR_SPEC_BASE_COLOUR_MAP " << value(PbrMaterialFeature::BaseColourMap) << '\n'
			<< "#define PBR_SPEC_METALLIC " << value(PbrMaterialFeature::Metallic) << '\n'
			<< "#define PBR_SPEC_ROUGHNESS " << value(PbrMaterialFeature::Roughness) << '\n'
			<< "#define PBR_SPEC_METALLIC_ROUGHNESS_MAP " << value(PbrMaterialFeature::MetallicRoughnessMap) << '\n'
			<< "#define PBR_SPEC_METALLIC_MAP " << value(PbrMaterialFeature::MetallicMap) << '\n'
			<< "#define PBR_SPEC_ROUGHNESS_MAP " << value(PbrMaterialFeature::RoughnessMap) << '\n'
			<< "#define PBR_SPEC_NORMAL_MAP " << value(PbrMaterialFeature::NormalMap) << '\n'
			<< "#define PBR_SPEC_OCCLUSION " << value(PbrMaterialFeature::Occlusion) << '\n'
			<< "#define PBR_SPEC_EMISSIVE " << value(PbrMaterialFeature::Emissive) << '\n'
			<< "#define PBR_SPEC_EMISSIVE_MAP " << value(PbrMaterialFeature::EmissiveMap) << '\n'
			<< "#define PBR_SPEC_ALPHA_MASK " << value(PbrMaterialFeature::AlphaMask) << '\n'
			<< "#define PBR_SPEC_ALPHA_BLEND " << value(PbrMaterialFeature::AlphaBlend) << '\n'
			<< "#define PBR_SPEC_DOUBLE_SIDED " << value(PbrMaterialFeature::DoubleSided) << '\n';
		return source.str();
	}

	string injectPbrSpecializationDefines(string const& shaderSource, PbrMaterialFeatures features)
	{
		auto const marker = shaderSource.find("@@Version");
		if (marker == string::npos) THROW_MPP("PBR shader specialization requires an @@Version directive.", __LINE__, __FILE__, __func__);
		auto const insertion = shaderSource.find('\n', marker);
		if (insertion == string::npos) return shaderSource + "\n" + makePbrSpecializationDefines(features);
		return shaderSource.substr(0, insertion + 1) + makePbrSpecializationDefines(features) + shaderSource.substr(insertion + 1);
	}
}
