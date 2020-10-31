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
	public:

		struct ProgramOptions
		{
			struct Shader
			{
				bool isFile;
				std::string data;
			};

			bool resourceExists{ false };
			std::string existingResource;
			std::set<std::string> tags;

			bool is2d;
			mesh::MeshSpecification spec;
			Shader vertexShader, fragmentShader;
		};

	protected:

		std::string mName;
 
		ProgramOptions mProgram;

		UniformCollection mUniforms;

		std::map<std::string, std::pair<std::string, bool>> mTextures;

	public:

		explicit MaterialStream(ResourceManager* resourceMgr);

		MaterialStream(ResourceManager* resourceMgr, std::string const& program);

		MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, std::string const& vertexShader, bool vertexShaderIsFile, std::string const& fragmentShader, bool fragmentShaderIsFile);

		MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec);

		MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, std::set<std::string> const& tags);

		std::string const& getName() const;

		void setProgram(std::string const& program);

		void setProgram(bool is2d, mesh::MeshSpecification const& spec, std::set<std::string> const& tags);

		void setProgram(bool is2d, mesh::MeshSpecification const& spec, std::string const& vertexShader, bool vertexShaderIsFile, std::string const& fragmentShader, bool fragmentShaderIsFiles);

		void setProgram(bool is2d, mesh::MeshSpecification const& spec);

		ProgramOptions const& getProgramOptions() const;

		UniformCollection const& getUniforms() const;

		std::map<std::string, std::pair<std::string, bool>> const& getTextures() const;
	};
}