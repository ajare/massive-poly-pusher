#pragma once

#include <exception>
#include <string>

namespace mpp
{
	namespace mesh
	{
		class MppMeshException : public std::exception
		{
		public:

			MppMeshException(std::string const& msg)
				: std::exception(msg.c_str())
			{
			}

		};
	}
}