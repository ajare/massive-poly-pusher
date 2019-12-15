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
		uint32 initialCount,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Model(name, renderSystem, resourceMgr, nullptr)
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
		auto programResource = resourceMgr->getOrCreateDefault2dProgram(mSpecification, programFlags, false);

		return createMaterial(name, programResource, texture, programFlags);
	}

	ResourcePtr Batch::createMaterial(string const& name, ResourcePtr program, string const& texture, uint32 programFlags)
	{
		auto resourceMgr = getResourceManager();

		ProgrammaticMaterialStream* matStream = new ProgrammaticMaterialStream();

		matStream->setProgram(program->getName());

		matStream->setTexture("tex", texture);

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
}