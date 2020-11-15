#pragma once

#include <vector>
#include <fstream>

#include "mpp/ResourceStream.h"

#include "mpp/program/Parser.h"
#include "mpp/program/Attribute.h"

namespace mpp
{
	class _MPPAPI ProgramStream : public ResourceStream
	{
		struct QualitySetting
		{
			std::shared_ptr<program::Parser> parser;
		};

	private:

		std::string mVertexSource, mFragmentSource;

	protected:

		std::vector<QualitySetting> mQualitySettings;

		std::set<std::string> mAttribs;

	private:

		void loadImpl();

	protected:

		void setVertexSource(std::string const& src);

		void setFragmentSource(std::string const& src);

	public:

		explicit ProgramStream(ResourceManager* resourceMgr);

		std::string const& getVertexSource() const;

		std::string const& getFragmentSource() const;

		std::string getConcatenatedSource();

		std::vector<program::Attribute> getInAttributes() const;

		std::vector<std::string> getUniforms() const;

		std::vector<std::string> getTextures() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}