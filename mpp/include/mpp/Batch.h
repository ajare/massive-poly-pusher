#pragma once

#include <vector>

#include "mpp/Model.h"

#include "mpp/mesh/Vertex.h"
#include "mpp/mesh/MeshSpecification.h"

namespace mpp
{

	class _MPPAPI Batch : public Model
	{
		std::string mDefaultVertexShader, mDefaultFragmentShader;

		std::string mProgramDescriptor;

	protected:

		size_t mCurCount, mMaxCount;

		mesh::MeshSpecification mSpecification;

		std::map<std::string, std::pair<char*, size_t>> mDataPointers;

	private:

		virtual bool indexedVertices() const = 0;

		virtual void createIndexData(std::vector<uint8>& data, uint32_t start, size_t count);

	protected:

		virtual mesh::Primitive::Type getPrimitiveType() const = 0;

		virtual void createMeshSpecification(mesh::Primitive::Type primitiveType) = 0;
		
		void createMesh(Mesh* mesh, size_t vertexCount, size_t bufferSize, std::shared_ptr<const int8> dataPtr);

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
			RenderSystem* renderSystem,
			ResourceManager* resourceMgr);

		virtual ~Batch() = default;

		mesh::MeshSpecification const& getSpecification() const;

		const std::pair<char*, size_t>& getAttributeData(std::string const& name) const;

		int getCount() const;

		int getCapacity() const;

		virtual int getPrimitiveCount(int objectCount) const;

		virtual int getVertexCount(int primitiveCount) = 0;

		void startUpdate(int minimumCount);

		virtual void finishUpdate(int count, bool updateTexCoords) = 0;
	};
}
#pragma once
