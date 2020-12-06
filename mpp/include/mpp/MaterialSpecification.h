#pragma once

#include <string>
#include <map>
#include <vector>

#include "mpp/TextureParams.h"
#include "mpp/UniformCollection.h"

#include <mpp/mesh/MeshSpecification.h>

namespace mpp
{

	struct MaterialSpecification
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
		UniformCollection uniforms;
		std::vector<TextureOptions> textures;
	};

}