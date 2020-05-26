#include <cassert>

#include "mpp/mesh/VertexBufferAttributeLayout.h"

namespace mpp
{
	namespace mesh
	{

		VertexBufferAttributeLayout::VertexBufferAttributeLayout()
			: mVertexSize(0)
		{
		}

		/*
		 * Create a channel in the vertex buffer specification.
		 *
		 */
		void VertexBufferAttributeLayout::createAttribute(Vertex::Component component, Vertex::DataType dataType, bool normalised, int paddingBytes)
		{
			createAttribute(mAttributes.size(), component, dataType, normalised, paddingBytes);
		}

		void VertexBufferAttributeLayout::createAttribute(uint32_t attribId, Vertex::Component component, Vertex::DataType dataType, bool normalised, int paddingBytes)
		{
			Attribute attrib;

			attrib.attributeId = attribId;
			attrib.component = component;
			attrib.dataType = dataType;
			attrib.normalised = normalised;
			attrib.paddingBytes = paddingBytes;

			attrib.sizeInBytes = Vertex::getComponentSize(component) * Vertex::getDataTypeSize(dataType) + paddingBytes;
			attrib.offsetInBytes = mAttributes.empty() ? 0 : mAttributes.back().offsetInBytes + mAttributes.back().sizeInBytes;

			mAttributes.push_back(attrib);
			mVertexSize += (Vertex::getComponentSize(component) * Vertex::getDataTypeSize(dataType));
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
