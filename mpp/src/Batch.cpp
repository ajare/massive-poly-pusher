#include <cmath>

#include "mpp/Batch.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	/*
	 * Constructor.
	 *
	 */
	Batch::Batch(std::string const& name,
		ColourOptions colourOptions,
		bool useDiffuseColour,
		uint32_t initialCount,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		string const& descriptor,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Model(name, renderSystem, resourceMgr, nullptr)
		, mDefaultVertexShader(defaultVertexShader)
		, mDefaultFragmentShader(defaultFragmentShader)
		, mProgramDescriptor(descriptor)
		, mColourOptions(colourOptions)
		, mUseDiffuse(useDiffuseColour)
		, mCurCount(0)
		, mMaxCount(initialCount)
		, mMainBufferStride(0)
		, mColourOffset(0)
		, mPositionData(nullptr)
		, mTexCoordData(nullptr)
		, mColourData(nullptr)
	{
	}

	/*
	 * Write a byte to a datastream.
	 *
	 */
	void Batch::writeUByte(uint8 value, int8** ptr)
	{
		**ptr = value;
		*ptr += sizeof(uint8);
	}

	/*
	 * Write a float to a datastream.
	 *
	 */
	void Batch::writeFloat(float value, int8** ptr)
	{
		float* fPtr = (float*)*ptr;
		*fPtr = value;
		*ptr += sizeof(float);
	}

	mesh::MeshSpecification const& Batch::getSpecification() const
	{
		return mSpecification;
	}

	char* Batch::getPositionData()
	{
		return mPositionData;
	}

	int Batch::getPositionDataSize() const
	{
		return (int)mMeshes[0]->getVertexBuffer(0)->getBufferData().size();
	}

	char* Batch::getTexCoordData()
	{
		return mTexCoordData;
	}

	int Batch::getTexcoordDataSize() const
	{
		return (int)mMeshes[0]->getVertexBuffer(1)->getBufferData().size();
	}

	char* Batch::getColourData()
	{
		return mColourData;
	}

	bool Batch::usingColour() const
	{
		return mColourOptions != ColourOptions::None;
	}

	void Batch::setCount(int count)
	{
		mCurCount = count;
	}

	int Batch::getCount() const
	{
		return mCurCount;
	}

	int Batch::getMaxCount() const
	{
		return mMaxCount;
	}

	void Batch::startUpdate(int minimumCount)
	{
		setMinimumCount(minimumCount);
	}

	ResourcePtr Batch::createMaterial(string const& name, string const& texture, uint32 programFlags)
	{
		auto resourceMgr = getResourceManager();
		auto programResource = resourceMgr->getOrCreateDefault2dProgram(mDefaultVertexShader, mDefaultFragmentShader, mSpecification, programFlags, false, mProgramDescriptor);

		return createMaterial(name, programResource, texture, programFlags);
	}

	ResourcePtr Batch::createMaterial(string const& name, ResourcePtr program, string const& texture, uint32 programFlags)
	{
		auto resourceMgr = getResourceManager();

		ProgrammaticMaterialStream* matStream = new ProgrammaticMaterialStream();

		matStream->setProgram(program->getName());

		matStream->setTexture("TEX1", texture);

		auto materialResource = resourceMgr->getResource(name, true);
		if (materialResource)
		{
			materialResource->load();
		}
		else
		{
			materialResource = resourceMgr->createResource<mpp::Material>(name, mpp::ResourceStreamPtr(matStream));
			materialResource->load();
		}

		return materialResource;
	}

	int Batch::getPrimitiveCount() const
	{
		return getCount() * 1;
	}

	/************************************************************************************/
	/************************************************************************************/
	/************************************************************************************/

	/*
	 * Constructor.
	 *
	 */
	Batch2::Batch2(std::string const& name,
		size_t initialCapacity,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		string const& descriptor,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Model(name, renderSystem, resourceMgr, nullptr)
		, mDefaultVertexShader(defaultVertexShader)
		, mDefaultFragmentShader(defaultFragmentShader)
		, mProgramDescriptor(descriptor)
		, mCurCount(0)
		, mMaxCount(initialCapacity)
	{
	}

	mesh::MeshSpecification const& Batch2::getSpecification() const
	{
		return mSpecification;
	}

	const pair<char*, size_t>& Batch2::getAttributeData(string const& name) const
	{
		return mDataPointers.at(name);
	}

	void Batch2::createMesh(Mesh* mesh, size_t vertexCount, size_t bufferSize, shared_ptr<const int8> dataPtr)
	{
		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto& layout = mSpecification.getVertexBufferAttributeLayout(i);
			auto vb = mesh->createVertexBuffer(vertexCount, bufferSize, false, dataPtr);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto& attrib = layout.getAttribute(j);
				vb->setAttribute(
					attrib.attributeId,
					attrib.dataType,
					mesh::Vertex::getComponentSize(attrib.component),
					attrib.offsetInBytes,
					attrib.normalised);
			}
		}

		setSpecificationPointers(mesh);
		mMeshes.push_back(mesh);
	}

	void Batch2::createIndexData(vector<uint8>& data, uint32_t start, size_t count)
	{
	}

	int Batch2::getCount() const
	{
		return mCurCount;
	}

	int Batch2::getCapacity() const
	{
		return mMaxCount;
	}

	void Batch2::startUpdate(int minimumCount)
	{
		setMinimumCount(minimumCount);
	}

	ResourcePtr Batch2::createMaterial(string const& name, string const& texture, uint32 programFlags)
	{
		auto resourceMgr = getResourceManager();
		auto programResource = resourceMgr->getOrCreateDefault2dProgram(mDefaultVertexShader, mDefaultFragmentShader, mSpecification, programFlags, false, mProgramDescriptor);

		return createMaterial(name, programResource, texture, programFlags);
	}

	ResourcePtr Batch2::createMaterial(string const& name, ResourcePtr program, string const& texture, uint32 programFlags)
	{
		auto resourceMgr = getResourceManager();

		ProgrammaticMaterialStream* matStream = new ProgrammaticMaterialStream();

		matStream->setProgram(program->getName());

		matStream->setTexture("TEX1", texture);

		auto materialResource = resourceMgr->getResource(name, true);
		if (materialResource)
		{
			materialResource->load();
		}
		else
		{
			materialResource = resourceMgr->createResource<mpp::Material>(name, mpp::ResourceStreamPtr(matStream));
			materialResource->load();
		}

		return materialResource;
	}

	int Batch2::getPrimitiveCount(int objectCount) const
	{
		return objectCount;
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void Batch2::setSpecificationPointers(Mesh* mesh)
	{
		auto buffers = mesh->getVertexBuffers();

		for (int i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto& layout = mSpecification.getVertexBufferAttributeLayout(i);

			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto& attrib = layout.getAttribute(j);
				if (buffers[i]->getBufferData().size() > 0)
				{
					auto dataPtr = (char*)&((buffers[i]->getBufferData()[0])) + attrib.offsetInBytes;
					mDataPointers[attrib.identifier] = make_pair(dataPtr, layout.getVertexSize());
				}
			}
		}
	}

	void Batch2::setMinimumCount(size_t count)
	{
		if (count > mMaxCount)
		{
			for (int i = 0; i < mMeshes[0]->getNumVertexBuffers(); ++i)
			{
				auto vertexBuffer = mMeshes[0]->getVertexBuffer(i);
				auto& data = vertexBuffer->getBufferData();

				int newSize = getVertexCount(getPrimitiveCount(count)) * vertexBuffer->getVertexStride();
				data.resize(newSize);

				// Index data
				if (indexedVertices())
				{
					createIndexData(mMeshes[0]->getIndexData(), mMaxCount, count);
				}
			}

			mMaxCount = count;
			setSpecificationPointers(mMeshes[0]);
		}
	}
}