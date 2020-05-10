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

		template<typename T>
		struct Uniform
		{
			T values[4];
			int valueCount;
		};

	protected:

		std::string mName;
 
		std::string mProgram;

		std::map<std::string, Uniform<float>> mFloatUniforms;

		std::map<std::string, std::string> mTextures;

	public:

		MaterialStream();

		explicit MaterialStream(std::string const& program);

		MaterialStream(bool program2d, mesh::MeshSpecification const& meshSpec);

		MaterialStream(bool program2d, mesh::MeshSpecification const& meshSpec, std::set<std::string> const& tags);

		std::string getType();

		std::string const& getName() const;

		void setProgram(std::string const& program);

		void setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, std::set<std::string> const& tags);

		std::string const& getProgram() const;

		std::map<std::string, Uniform<float>> const& getFloatUniforms() const;

		std::map<std::string, std::string> const& getTextures() const;
	};
}