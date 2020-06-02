#include "mpp/program/Parser.h"

using namespace std;

namespace mpp
{
	namespace program
	{

		/*
		 * Constructor.
		 *
		 */
		Parser::Parser()
		{
		}

		/*
		 * Set vertex shader source.
		 *
		 */
		void Parser::setVertexSource(string const& src)
		{
			mVertexSource = src;
		}

		/*
		 * Set fragment shader source.
		 *
		 */		
		void Parser::setFragmentSource(string const& src)
		{
			mFragmentSource = src;
		}

		/*
		 * Set mesh specification to build program with.
		 *
		 */
		void Parser::setMeshSpecification(mesh::MeshSpecification const& spec)
		{
			mSpecification = spec;
		}

		/*
		 * Parse files and get information, check against spec, and build final sources.
		 *
		 */
		void Parser::build()
		{

		}
	}
}