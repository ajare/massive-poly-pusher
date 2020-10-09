#pragma once

#include <vector>
#include <fstream>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI ProgramStream : public ResourceStream
	{
		std::string mVertexSource, mFragmentSource;

	protected:

		void setVertexSource(std::string const& src);

		void setFragmentSource(std::string const& src);

	public:

		ProgramStream();

		std::string getType();

		std::string const& getVertexSource() const;

		std::string const& getFragmentSource() const;

		std::string getConcatenatedSource();
	};
}