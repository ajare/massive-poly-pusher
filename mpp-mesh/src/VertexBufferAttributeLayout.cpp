#include <cassert>

#include "mpp/mesh/VertexBufferAttributeLayout.h"

namespace mpp
{
	namespace mesh
	{

		VertexBufferAttributeLayout::VertexBufferAttributeLayout(int baseId)
			: mVertexSize(0)
			, mBaseId(baseId)
		{
		}

		/*
		 * Create a channel in the vertex buffer specification.
		 *
		 */
		void VertexBufferAttributeLayout::createAttribute(Vertex::Component component, Vertex::DataType dataType, bool normalised, int padToBoundary)
		{
			Attribute attrib;

			attrib.attributeId = mBaseId + mAttributes.size();
			attrib.component = component;
			attrib.dataType = dataType;
			attrib.normalised = normalised;

			if (padToBoundary < 2)
			{
				attrib.paddingBytes = 0;
			}
			else
			{
				auto byteSize = Vertex::getComponentSize(component) * Vertex::getDataTypeSize(dataType);
				auto padding = padToBoundary;
				while (padding < byteSize)
				{
					padding += padToBoundary;
				}
				
				attrib.paddingBytes = padding - byteSize;
			}

			attrib.offsetInBytes = mVertexSize;

			mAttributes.push_back(attrib);
			mVertexSize += attrib.sizeInBytes();
		}

		/*
		 * Get the base id.
		 *
		 */
		int VertexBufferAttributeLayout::getBaseId() const
		{
			return mBaseId;
		}

		/*
		 * Get the number of channels.
		 *
		 */
		int VertexBufferAttributeLayout::getNumAttributes() const
		{
			return (int)mAttributes.size();
		}

		/*
		 * Get the specified channel.
		 *
		 */
		VertexBufferAttributeLayout::Attribute const& VertexBufferAttributeLayout::getAttribute(int index) const
		{
			assert((index >= 0 && index < getNumAttributes()) && "VertexBufferAttributeLayout::getAttribute() 'index' argument out of range!");
			return mAttributes[index];
		}

		/*
		 * Get the specified channel.
		 *
		 */
		VertexBufferAttributeLayout::Attribute& VertexBufferAttributeLayout::getAttribute(int index)
		{
			assert((index >= 0 && index < getNumAttributes()) && "VertexBufferAttributeLayout::getAttribute() 'index' argument out of range!");
			return mAttributes[index];
		}

		int VertexBufferAttributeLayout::getVertexSize() const
		{
			return mVertexSize;
		}
	}
}
