#pragma once

#include <vector>
#include "Config.h"
#include "Vertex.h"

namespace mpp
{
	namespace mesh
	{

		class _MPPMESHAPI VertexBufferAttributeLayout
		{
		public:

			struct Attribute
			{
				int attributeId;
				Vertex::Component component;
				Vertex::DataType dataType;
				int paddingBytes;
				bool normalised;

				int offsetInBytes;

			public:

				size_t sizeInBytes() const
				{
					return Vertex::getComponentSize(component) * Vertex::getDataTypeSize(dataType) + paddingBytes;
				}
			};

		private:

			std::vector<Attribute> mAttributes;

			int mVertexSize;

		public:

			VertexBufferAttributeLayout();

			void createAttribute(Vertex::Component component, Vertex::DataType dataType, bool normalised, int paddingBytes = 0);

			void createAttribute(uint32_t attribId, Vertex::Component component, Vertex::DataType dataType, bool normalised, int paddingBytes = 0);

			int getNumAttributes() const;

			Attribute const& getAttribute(int index) const;

			Attribute& getAttribute(int index);

			int getVertexSize() const;
		};

	}
}