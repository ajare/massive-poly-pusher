#pragma once

#include <vector>
#include <map>
#include <set>

#include "mpp/ResourceStream.h"
#include "mpp/FileDataStream.h"

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	class _MPPAPI MaterialStream : public ResourceStream
	{
	public:

		struct ProgramOptions
		{
			bool resourceExists{ false };
			std::string existingResource;
			std::set<std::string> tags;

			bool is2d, useDefaultShaders, shadersAreFiles;
			mesh::MeshSpecification spec;
			std::string vertexShader, fragmentShader;
		};

		template<typename T>
		struct Uniform
		{
			T values[4];
			int valueCount;
		};

	protected:

		std::string mName;
 
		ProgramOptions mProgram;

		std::map<std::string, Uniform<float>> mFloatUniforms;

		std::map<std::string, std::string> mTextures;

	public:

		explicit MaterialStream(ResourceManager* resourceMgr);

		MaterialStream(ResourceManager* resourceMgr, std::string const& program);

		MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, std::string const& vertexShader, std::string const& fragmentShader, bool shadersAreFiles);

		MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec);

		MaterialStream(ResourceManager* resourceMgr, bool program2d, mesh::MeshSpecification const& meshSpec, std::set<std::string> const& tags);

		std::string getType();

		std::string const& getName() const;

		void setProgram(std::string const& program);

		void setProgram(bool is2d, mesh::MeshSpecification const& spec, std::set<std::string> const& tags);

		void setProgram(bool is2d, mesh::MeshSpecification const& spec, std::string const& vertexShader, std::string const& fragmentShader, bool shadersAreFiles);

		void setProgram(bool is2d, mesh::MeshSpecification const& spec);

		ProgramOptions const& getProgramOptions() const;

		std::map<std::string, Uniform<float>> const& getFloatUniforms() const;

		std::map<std::string, std::string> const& getTextures() const;
	};
}