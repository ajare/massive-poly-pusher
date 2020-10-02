#pragma once

#include <vector>

#include "mpp/Model.h"

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{

	struct BatchVertexAttribute
	{
		mesh::Vertex::DataType dataType;
		bool fixedValues;
	};

	class _MPPAPI Batch : public Model
	{
		std::string mDefaultVertexShader, mDefaultFragmentShader;

		std::string mProgramDescriptor;

		BatchVertexAttribute mColourAttrib;
		
		bool mUseDiffuse;

		static std::pair<char*, size_t> msNonExistentAttribute;

	protected:

		size_t mCurCount, mMaxCount;

		mesh::MeshSpecification mSpecification;

		std::map<std::string, std::pair<char*, size_t>> mDataPointers;

	private:

		virtual bool indexedVertices() const = 0;

		virtual void createIndexData(std::vector<uint8>& data, uint32_t start, size_t count);

	protected:

		BatchVertexAttribute getColourAttribute() const;

		virtual mesh::Primitive::Type getPrimitiveType() const = 0;

		virtual void createMeshSpecification(mesh::Primitive::Type primitiveType) = 0;
		
		void createVertexBuffer(uint32 index, Mesh* mesh, size_t vertexCount, bool staticData);

		void setSpecificationPointers(Mesh* mesh);

		virtual void setMinimumCount(size_t count);

		ResourcePtr createMaterial(std::string const& name, std::string const& texture, uint32 programFlags);

		ResourcePtr createMaterial(std::string const& name, ResourcePtr program, std::string const& texture, uint32 programFlags);

	public:

		Batch(std::string const& name,
			size_t initialCapacity,
			std::string const& defaultVertexShader,
			std::string const& defaultFragmentShader,
			std::string const& descriptor,
			BatchVertexAttribute colourAttrib,
			bool useDiffuse,
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		virtual ~Batch() = default;

		mesh::MeshSpecification const& getSpecification() const;

		const std::pair<char*, size_t>& getAttributeData(std::string const& name) const;

		size_t getCount() const;

		size_t getCapacity() const;

		virtual size_t getPrimitiveCount(size_t objectCount) const;

		virtual size_t getVertexCount(size_t primitiveCount) = 0;

		void startUpdate(size_t minimumCount);

		virtual void finishUpdate(size_t count, bool updateFixedBuffers);

		bool usingDiffuse() const;

		bool usingColour() const;
	};
}
#pragma once
