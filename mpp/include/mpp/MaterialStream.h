#pragma once

#include <vector>
#include <map>

#include "mpp/ResourceStream.h"
#include "mpp/FileDataStream.h"

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

		std::string getType();

		std::string const& getName() const;

		std::string const& getProgram() const;

		std::map<std::string, Uniform<float>> const& getFloatUniforms() const;

		std::map<std::string, std::string> const& getTextures() const;
	};
}