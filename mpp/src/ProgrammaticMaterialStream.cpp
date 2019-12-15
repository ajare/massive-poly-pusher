#include "mpp/Program.h"
#include "mpp/ProgrammaticMaterialStream.h"

using namespace std;

namespace mpp
{

	void ProgrammaticMaterialStream::setProgram(string const& program)
	{
		mProgram = program;
	}

	void ProgrammaticMaterialStream::setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, set<string> const& tags)
	{
		mProgram = "__mpp_";
		mProgram += is2d ? "p2d_" : "p3d_";
		
		switch (spec.getPrimitiveType())
		{
		case mesh::Primitive::Type::Lines:
		case mesh::Primitive::Type::Triangles:
			mProgram += "tris_";
			break;
		case mesh::Primitive::Type::Points:
			mProgram += "points_";
			break;
		}

		for (int i = 0; i < spec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = spec.getVertexBufferAttributeLayout(i);
			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attr = layout.getAttribute(j);
				switch (attr.component)
				{
				case mesh::Vertex::Component::Position2:
					mProgram += "p2";
					break;
				case mesh::Vertex::Component::Position3:
					mProgram += "p3";
					break;
				case mesh::Vertex::Component::Position4:
					mProgram += "p4";
					break;
				case mesh::Vertex::Component::Normal3:
					mProgram += "n3";
					break;
				case mesh::Vertex::Component::TexCoord2:
					mProgram += "t2";
					break;
				case mesh::Vertex::Component::TexCoord3:
					mProgram += "t3";
					break;
				case mesh::Vertex::Component::TexCoord4:
					mProgram += "t4";
					break;
				case mesh::Vertex::Component::Colour1:
					mProgram += "c1";
					break;
				case mesh::Vertex::Component::Colour3:
					mProgram += "c3";
					break;
				case mesh::Vertex::Component::Colour4:
					mProgram += "c4";
					break;
				}
			}
		}

		for (auto const& tag : tags)
		{
			if (tag == "diffuse")
			{
				mProgram += "d";
			}
		}
	}

	void ProgrammaticMaterialStream::setTexture(string const& sampler, string const& texture)
	{
		mTextures[sampler] = texture;
	}

	void ProgrammaticMaterialStream::useDefaultTexture()
	{
		mTextures["tex"] = "__mpp_tex_none";
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, float value)
	{
		Uniform<float> u;
		u.valueCount = 1;
		u.values[0] = value;

		// Mark up name
		string markedUpName = MPP_PROGRAM_MARKUP_UNIFORM(name);
		mFloatUniforms[markedUpName] = u;
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, glm::vec2 const& value)
	{
		Uniform<float> u;

		u.valueCount = 2;
		u.values[0] = value[0];
		u.values[1] = value[1];

		// Mark up name
		string markedUpName = MPP_PROGRAM_MARKUP_UNIFORM(name);
		mFloatUniforms[markedUpName] = u;
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, glm::vec3 const& value)
	{
		Uniform<float> u;

		u.valueCount = 3;
		u.values[0] = value[0];
		u.values[1] = value[1];
		u.values[2] = value[2];

		mFloatUniforms[name] = u;
	}

	void ProgrammaticMaterialStream::setFloatUniform(string const& name, glm::vec4 const& value)
	{
		Uniform<float> u;

		u.valueCount = 4;
		u.values[0] = value[0];
		u.values[1] = value[1];
		u.values[2] = value[2];
		u.values[3] = value[3];

		mFloatUniforms[name] = u;
	}

}