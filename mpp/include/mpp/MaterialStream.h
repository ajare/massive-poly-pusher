#pragma once

#include <vector>
#include <map>
#include <set>

#include "mpp/ResourceStream.h"
#include "mpp/FileDataStream.h"
#include "mpp/UniformCollection.h"

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	class _MPPAPI MaterialStream : public ResourceStream
	{
		friend class ResourceStreamSerializer;

	public:

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

	protected:

		struct QualitySetting
		{
			ProgramOptions program;
			UniformCollection uniforms;
			std::map<std::string, std::pair<std::string, bool>> textures;
		};

	protected:

		std::string mName;
 
		std::vector<QualitySetting> mQualitySettings;

	public:

		explicit MaterialStream(ResourceManager* resourceMgr);

		std::string const& getName() const;

		ProgramOptions const& getProgramOptions() const;

		UniformCollection const& getUniforms() const;

		std::map<std::string, std::pair<std::string, bool>> const& getTextures() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}