#pragma once

#include <string>
#include <map>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#pragma warning(pop)

#include "mpp/TextureParams.h"
#include "mpp/UniformCollection.h"

#include <mpp/mesh/MeshSpecification.h>

namespace mpp
{

	struct PbrMaterialSpecification
	{
		struct ProgramOptions
		{
			bool resourceExists{ false };

			// For an existing program resource
			std::string existingResource;
			bool isChild{ false };

			// Info for creating new resource
			struct Shader
			{
				enum class Type
				{
					Default,
					File,
					Resource
				};

				Type type{ Type::Default };
				std::string data;
			};

			bool is2d;
			mesh::MeshSpecification spec;
			Shader vertexShader, geometryShader, fragmentShader;
		};

		enum class PbrAlphaMode
		{
			Opaque,
			Mask,
			Blend
		};

		struct PbrSurface
		{
			bool enabled{ false };
			glm::vec4 baseColourFactor{ 1.0f };
			float metallicFactor{ 1.0f };
			float roughnessFactor{ 1.0f };
			glm::vec3 emissiveFactor{ 0.0f };
			float normalScale{ 1.0f };
			float occlusionStrength{ 1.0f };
			PbrAlphaMode alphaMode{ PbrAlphaMode::Opaque };
			float alphaCutoff{ 0.5f };
			bool doubleSided{ false };
		};

		struct TextureOptions
		{
			bool resourceExists{ false };
			std::string sampler;

			// For an existing program resource
			std::string existingResource;
			bool isChild{ false };

			// Info for creating new resource
			std::string source;
			TextureTarget target{ TextureTarget::Texture2D };

			TextureParams params;
		};

	public:

		ProgramOptions program;
		// Runtime-only compatibility marker. It is deliberately not serialized.
		bool legacyFullContract{ false };
		UniformCollection uniforms;
		PbrSurface pbr;
		std::vector<TextureOptions> textures;
	};

}