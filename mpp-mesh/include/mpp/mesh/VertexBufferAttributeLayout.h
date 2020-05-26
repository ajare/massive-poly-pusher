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

			int mBaseId;

		public:

			explicit VertexBufferAttributeLayout(int baseId);

			void createAttribute(Vertex::Component component, Vertex::DataType dataType, bool normalised, int padToBoundary = 1);

			int getBaseId() const;

			int getNumAttributes() const;

			Attribute const& getAttribute(int index) const;

			Attribute& getAttribute(int index);

			int getVertexSize() const;
		};

	}
}