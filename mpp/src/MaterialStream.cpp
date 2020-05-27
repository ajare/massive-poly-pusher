#include "mpp/MaterialStream.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream()
	{
	}
	
	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(string const& program)
		: MaterialStream()
	{
		setProgram(program);
	}

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(bool program2d, mesh::MeshSpecification const& meshSpec)
		: MaterialStream()
	{
		setProgram(program2d, meshSpec, {});
	}

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(bool program2d, mesh::MeshSpecification const& meshSpec, set<string> const& tags)
		: MaterialStream()
	{
		setProgram(program2d, meshSpec, tags);
	}

	/*
	 * Get resource type.
	 *
	 */
	std::string MaterialStream::getType()
	{
		return "Material";
	}

	/*
	 * Get name
	 *
	 */
	string const& MaterialStream::getName() const
	{
		return mName;
	}

	/*
	 * Set program
	 *
	 */
	void MaterialStream::setProgram(string const& program)
	{
		mProgram = program;
	}

	/*
	 * Set program
	 *
	 */
	void MaterialStream::setProgram(bool is2d, mpp::mesh::MeshSpecification const& spec, set<string> const& tags)
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
				case mesh::Vertex::Component::Normal4:
					mProgram += "n4";
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

	/*
	 * Get program.
	 *
	 */
	string const& MaterialStream::getProgram() const
	{
		return mProgram;
	}

	/*
	 * Get program uniforms.
	 *
	 */
	map<string, MaterialStream::Uniform<float>> const& MaterialStream::getFloatUniforms() const
	{
		return mFloatUniforms;
	}

	/*
	 * Get textures.
	 *
	 */
	map<string, string> const& MaterialStream::getTextures() const
	{
		return mTextures;
	}
}