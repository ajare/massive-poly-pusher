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
		std::string mVertexSource, mFragmentSource;

		std::shared_ptr<program::Parser> mParser;

		std::set<std::string> mAttribs;

	private:

		void loadImpl();

	protected:

		void setVertexSource(std::string const& src);

		void setFragmentSource(std::string const& src);

	public:

		ProgramStream(ResourceManager* resourceMgr, std::shared_ptr<program::Parser> parser, std::set<std::string> const& attribs);

		std::string getType();

		std::string const& getVertexSource() const;

		std::string const& getFragmentSource() const;

		std::string getConcatenatedSource();

		std::vector<program::Attribute> getInAttributes() const;

		std::vector<std::string> getUniforms() const;

		std::vector<std::string> getTextures() const;
	};
}