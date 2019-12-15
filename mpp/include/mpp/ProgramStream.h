#pragma once

#include <vector>
#include <fstream>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI ProgramStream : public ResourceStream
	{
	public:

		struct AttribInfo
		{
			std::string name;
			int size, offset;
			bool normalise;
		};

	private:

		std::string mVertexSource, mFragmentSource;

		std::vector<AttribInfo> mAttribInfo;

	protected:

		void setVertexSource(std::string const& src);

		void setFragmentSource(std::string const& src);

	public:

		ProgramStream();

		std::string getType();

		void loadFromStrings(std::string const& vertSrc, std::string const& fragSrc);

		void loadFromFiles(std::string const& vertFile, std::string const& fragFile);

		void addAttribInfo(std::string const& name, int size, int offset, bool normalise);

		std::vector<AttribInfo> const& getAttribInfo() const;
		
		std::string const& getVertexSource() const;

		std::string const& getFragmentSource() const;
	};
}