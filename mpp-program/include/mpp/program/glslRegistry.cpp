#include <map>
#include <string>

#include "glslTypes.h"
#include "MppProgramException.h"

namespace mpp
{
	namespace program
	{

		using namespace std;

		map<string, GLSLTypeDecl> gsGLSLTypeDecls
		{
			{"bool", {GLSLType::Bool, {1, 1}, false, true}},
			{"int",  {GLSLType::Int, {1, 1}, false, true}},
			{"uint", {GLSLType::Uint, {1, 1}, false, false}},
			{"float", {GLSLType::Float, {1, 1}, true, true}},
			{"double", {GLSLType::Double, {1, 1}, true, true}},

			{"bvec2", {GLSLType::Bool, {2, 1}, false, true}},
			{"ivec2",  {GLSLType::Int, {2, 1}, false, true}},
			{"uvec2", {GLSLType::Uint, {2, 1}, false, false}},
			{"vec2", {GLSLType::Float, {2, 1}, true, true}},
			{"dvec2", {GLSLType::Double, {2, 1}, true, true}},

			{"bvec3", {GLSLType::Bool, {3, 1}, false, true}},
			{"ivec3",  {GLSLType::Int, {3, 1}, false, true}},
			{"uvec3", {GLSLType::Uint, {3, 1}, false, false}},
			{"vec3", {GLSLType::Float, {3, 1}, true, true}},
			{"dvec3", {GLSLType::Double, {3, 1}, true, true}},

			{"bvec4", {GLSLType::Bool, {4, 1}, false, true}},
			{"ivec4",  {GLSLType::Int, {4, 1}, false, true}},
			{"uvec4", {GLSLType::Uint, {4, 1}, false, false}},
			{"vec4", {GLSLType::Float, {4, 1}, true, true}},
			{"dvec4", {GLSLType::Double, {4, 1}, true, true}},

			{"mat2", {GLSLType::FloatMatrix, {2, 2}, true, true}},
			{"dmat2", {GLSLType::DoubleMatrix, {2, 2}, true, true}},

			{"mat3", {GLSLType::FloatMatrix, {3, 3}, true, true}},
			{"dmat3", {GLSLType::DoubleMatrix, {3, 3}, true, true}},

			{"mat4", {GLSLType::FloatMatrix, {4, 4}, true, true}},
			{"dmat4", {GLSLType::DoubleMatrix, {4, 4}, true, true}},

			{"mat2x3", {GLSLType::FloatMatrix, {2, 3}, true, true}},
			{"dmat2x3", {GLSLType::DoubleMatrix, {2, 3}, true, true}},

			{"mat2x4", {GLSLType::FloatMatrix, {2, 4}, true, true}},
			{"dmat2x4", {GLSLType::DoubleMatrix, {2, 4}, true, true}},

			{"mat3x2", {GLSLType::FloatMatrix, {3, 2}, true, true}},
			{"dmat3x2", {GLSLType::DoubleMatrix, {3, 2}, true, true}},

			{"mat3x4", {GLSLType::FloatMatrix, {3, 4}, true, true}},
			{"dmat3x4", {GLSLType::DoubleMatrix, {3, 4}, true, true}},

			{"mat4x2", {GLSLType::FloatMatrix, {4, 2}, true, true}},
			{"dmat4x2", {GLSLType::DoubleMatrix, {4, 2}, true, true}},

			{"mat4x3", {GLSLType::FloatMatrix, {4, 3}, true, true}},
			{"dmat4x3", {GLSLType::DoubleMatrix, {4, 3}, true, true}}
		};

		float getRealComponentIndexDefault(string const& component, int index, float def)
		{
			if (component == "POSITION")
			{
				switch (index)
				{
				case 2: 
					return 0.0f;

				case 3: 
					return 1.0f;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			else if (component == "NORMAL")
			{
				switch (index)
				{
				case 3:
					return 1.0f;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}

			}
			else if (component == "TEXCOORDS")
			{
				switch (index)
				{
				case 2:
					return 1.0f;

				case 3:
					return 1.0f;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}

			}
			else if (component == "COLOUR")
			{
				switch (index)
				{
				case 1:
					return def;

				case 2:
					return def;

				case 3:
					return 1.0f;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			else
			{
				THROW_MPP_PROGRAM("Unknown component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
			}
		}

		int getSignedComponentIndexDefault(string const& component, int index, int def)
		{
			if (component == "POSITION")
			{
				switch (index)
				{
				case 2:
					return 0;

				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			else if (component == "NORMAL")
			{
				switch (index)
				{
				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}

			}
			else if (component == "TEXCOORDS")
			{
				switch (index)
				{
				case 2:
					return 1;

				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}

			}
			else if (component == "COLOUR")
			{
				switch (index)
				{
				case 1:
					return def;

				case 2:
					return def;

				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			else
			{
				THROW_MPP_PROGRAM("Unknown component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
			}
		}

		unsigned int getUnsignedComponentIndexDefault(string const& component, int index, unsigned int def)
		{
			if (component == "POSITION")
			{
				switch (index)
				{
				case 2:
					return 0;

				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			else if (component == "NORMAL")
			{
				switch (index)
				{
				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}

			}
			else if (component == "TEXCOORDS")
			{
				switch (index)
				{
				case 2:
					return 1;

				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}

			}
			else if (component == "COLOUR")
			{
				switch (index)
				{
				case 1:
					return def;

				case 2:
					return def;

				case 3:
					return 1;

				default:
					THROW_MPP_PROGRAM("Bad index for component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
				}
			}
			else
			{
				THROW_MPP_PROGRAM("Unknown component '" + component + "'.", __LINE__, __FILE__, __FUNCTION__);
			}
		}
	}
}