#include <cassert>

#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{
	namespace mesh
	{
		using namespace std;

		/*
		 * Constructor.
		 *
		 */
		MeshSpecification::MeshSpecification()
			: mPrimitiveType(Primitive::Type::Triangles)
			, mStorageType(VertexBufferStorageType::Static)
			, mIndexedVertices(false)
		{
		}

		/*
		 * Constructor.
		 *
		 */
		MeshSpecification::MeshSpecification(Primitive::Type primitiveType)
			: mPrimitiveType(primitiveType)
			, mStorageType(VertexBufferStorageType::Static)
			, mIndexedVertices(false)
		{
		}

		/*
		 * Constructor.
		 *
		 */
		MeshSpecification::MeshSpecification(Primitive::Type primitiveType, VertexBufferStorageType storageType)
			: mPrimitiveType(primitiveType)
			, mStorageType(storageType)
			, mIndexedVertices(false)
		{
		}

		/*
		 * Copy constructor.
		 *
		 */
		MeshSpecification::MeshSpecification(MeshSpecification const& other)
		{
			this->mPrimitiveType = other.mPrimitiveType;
			this->mStorageType = other.mStorageType;
			this->mIndexedVertices = other.mIndexedVertices;
			this->mVertexBufferAttributeLayouts = other.mVertexBufferAttributeLayouts;
		}

		/*
		 * Set the primitive type, ie points/lines/triangles, etc.
		 *
		 */
		void MeshSpecification::setPrimitiveType(Primitive::Type primitiveType)
		{
			mPrimitiveType = primitiveType;
		}
		
		/*
		 * Get the primitive type, ie points/lines/triangles, etc.
		 *
		 */
		Primitive::Type MeshSpecification::getPrimitiveType() const
		{
			return mPrimitiveType;
		}

		/*
		 * Set the storage type, ie static or dynamic (for updating).
		 *
		 */
		void MeshSpecification::setStorageType(VertexBufferStorageType storageType)
		{
			mStorageType = storageType;
		}

		/*
		 * Get the storage type.
		 *
		 */
		VertexBufferStorageType MeshSpecification::getStorageType() const
		{
			return mStorageType;
		}

		/*
		 * Set vertices indexed.
		 *
		 */
		void MeshSpecification::setIndexedVertices(bool indexed)
		{
			mIndexedVertices = indexed;
		}

		/*
		 * Are vertices indexed?  If the primitive type is points, then there is no point in indexing them, it is just inefficient.
		 *
		 */
		bool MeshSpecification::verticesIndexed() const
		{
			return (mPrimitiveType != Primitive::Type::Points) && mIndexedVertices;
		}

		/*
		 * Create attribute layout.
		 *
		 */
		VertexBufferAttributeLayout* MeshSpecification::createVertexBufferAttributeLayout()
		{
			int baseId = mVertexBufferAttributeLayouts.empty() ? 0 
				: mVertexBufferAttributeLayouts.back().getBaseId() + mVertexBufferAttributeLayouts.back().getNumAttributes();
			mVertexBufferAttributeLayouts.push_back(VertexBufferAttributeLayout(baseId));
			return &(mVertexBufferAttributeLayouts.back());
		}

		/*
		 * Get number of attribute layouts.
		 *
		 */
		int MeshSpecification::getNumVertexBufferAttributeLayouts() const
		{
			return (int)mVertexBufferAttributeLayouts.size();
		}

		/*
	 	 * Get specified attribute layout.
		 *
		 */
		VertexBufferAttributeLayout const& MeshSpecification::getVertexBufferAttributeLayout(int index) const
		{
			assert((index >= 0 && index < getNumVertexBufferAttributeLayouts()) && "MeshSpecification::VertexBufferAttributeLayout::getVertexBufferAttributeLayout() 'index' argument out of range!");
			return mVertexBufferAttributeLayouts[index];
		}

		/*
		 * Get specified attribute layout.
		 *
		 */
		VertexBufferAttributeLayout& MeshSpecification::getVertexBufferAttributeLayout(int index) 
		{
			assert((index >= 0 && index < getNumVertexBufferAttributeLayouts()) && "MeshSpecification::VertexBufferAttributeLayout::getVertexBufferAttributeLayout() 'index' argument out of range!");
			return mVertexBufferAttributeLayouts[index];
		}

		/*
		 * Get a name.
		 *
		 */
		string MeshSpecification::getDescriptor(string const& prefix) const
		{
			// Generate name
			auto specHash = getHashCode();
			string specName = prefix;

			switch (specHash & 3)
			{
			case 1:
				specName += "points"; break;

			case 2:
				specName += "lines"; break;

			case 3:
				specName += "tris"; break;

			default:
				break;
			}

			int pc = (specHash >> 2) & 7;
			int nc = (specHash >> 5) & 7;
			int tc = (specHash >> 8) & 7;
			int cc = (specHash >> 11) & 7;

			if (pc || nc || tc || cc)
			{
				specName += "_";
			}

			if (pc > 0)
			{
				specName += "p" + to_string(pc);
			}
			if (nc > 0)
			{
				specName += "n" + to_string(nc);
			}
			if (tc > 0)
			{
				specName += "t" + to_string(tc);
			}
			if (cc > 0)
			{
				specName += "c" + to_string(cc);
			}

			return specName;
		}

		/*
		 * Gets a hash code.
		 *
		 */
		uint32 MeshSpecification::getHashCode() const
		{
			auto primType = getPrimitiveType();
			int primBits = (int)primType + 1;

			int posBits = 0, normalBits = 0, texBits = 0, colBits = 0, userBits = 0;
			for (int i = 0; i < getNumVertexBufferAttributeLayouts(); ++i)
			{
				auto const& layout = getVertexBufferAttributeLayout(i);
				for (int j = 0; j < layout.getNumAttributes(); ++j)
				{
					auto const& attrib = layout.getAttribute(j);

					switch (attrib.component)
					{
					case Vertex::Component::Position2:
						posBits = 2;
						break;
					case Vertex::Component::Position3:
						posBits = 3;
						break;
					case Vertex::Component::Position4:
						posBits = 4;
						break;
					case Vertex::Component::Normal3:
						normalBits = 3;
						break;
					case Vertex::Component::Normal4:
						normalBits = 4;
						break;
					case Vertex::Component::TexCoord2:
						texBits = 2;
						break;
					case Vertex::Component::TexCoord3:
						texBits = 3;
						break;
					case Vertex::Component::TexCoord4:
						texBits = 4;
						break;
					case Vertex::Component::Colour1:
						colBits = 1;
						break;
					case Vertex::Component::Colour3:
						colBits = 3;
						break;
					case Vertex::Component::Colour4:
						colBits = 4;
						break;
					case Vertex::Component::UserDefined1:
						userBits = 1;
						break;
					case Vertex::Component::UserDefined2:
						userBits = 2;
						break;
					case Vertex::Component::UserDefined3:
						userBits = 3;
						break;
					case Vertex::Component::UserDefined4:
						userBits = 4;
						break;
					default:
						break;
					}
				}
			}

			// Hash
			return
				(primBits << 0) +
				(posBits << 2) +
				(normalBits << 5) +
				(texBits << 8) +
				(colBits << 11) +
				(userBits << 14);
		}
	}
}

