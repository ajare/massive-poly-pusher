#include "mpp/mesh-specification-parser/MaterialInformation.h"

namespace mpp
{
	namespace mesh_specification_parser
	{

		using namespace std;

		MaterialInformation::MaterialInformation(string const& name)
			: mName(name)
			, mProgram("")
		{
		}

		string const& MaterialInformation::getName() const
		{
			return mName;
		}

		void MaterialInformation::setProgram(string const& program)
		{
			mProgram = program;
		}

		string const& MaterialInformation::getProgram() const
		{
			return mProgram;
		}
	}
}