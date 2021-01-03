#include <cmath>

#include "mpp/Batch.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	pair<char*, size_t> Batch::msNonExistentAttribute{ nullptr, 0 };

	/*
	 * Constructor.
	 *
	 */
	Batch::Batch(std::string const& name,
		size_t initialCapacity,
		string const& defaultVertexShader,
		string const& defaultFragmentShader,
		string const& descriptor,
		BatchVertexAttribute colourAttrib,
		bool useDiffuse,
		RenderSystem* renderSystem,
		ResourceManager* resourceMgr)
		: Model(name, renderSystem, resourceMgr, nullptr)
		, mDefaultVertexShader(defaultVertexShader)
		, mDefaultFragmentShader(defaultFragmentShader)
		, mProgramDescriptor(descriptor)
		, mCurCount(0)
		, mMaxCount(initialCapacity)
		, mColourAttrib(colourAttrib)
		, mUseDiffuse(useDiffuse)
	{
	}

	mesh::MeshSpecification const& Batch::getSpecification() const
	{
		return mSpecification;
	}

	const pair<char*, size_t>& Batch::getAttributeData(string const& name) const
	{
		auto it = mDataPointers.find(name);
		if (it == mDataPointers.end())
		{
			return msNonExistentAttribute;
		}

		return mDataPointers.at(name);
	}

	void Batch::createVertexBuffer(uint32_t index, Mesh* mesh, size_t vertexCount, bool staticData)
	{
		auto& layout = mSpecification.getVertexBufferAttributeLayout(index);

		auto bufferSize = layout.getVertexSize();
		int8_t* data = new int8_t[vertexCount * bufferSize];
		shared_ptr<const int8_t> dataPtr(data, [](int8_t*p) { delete[] p; });

		auto vb = mesh->createVertexBuffer(vertexCount, bufferSize, false, staticData, dataPtr);

		for (size_t j = 0; j < layout.getNumAttributes(); ++j)
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

	void Batch::createIndexData(vector<uint8_t>& data, uint32_t start, size_t count)
	{
	}

	size_t Batch::getCount() const
	{
		return mCurCount;
	}

	size_t Batch::getCapacity() const
	{
		return mMaxCount;
	}

	void Batch::startUpdate(size_t minimumCount)
	{
		setMinimumCount(minimumCount);
	}

	void Batch::finishUpdate(size_t count, bool updateFixedBuffers)
	{
		mCurCount = count;
		auto numPrimitives = getPrimitiveCount(count);

		if (numPrimitives > 0)
		{
			if (mMeshes[0]->isIndexed())
			{
				mMeshes[0]->mapIndexData(numPrimitives);
			}

			for (int i = 0; i < mMeshes[0]->getNumVertexBuffers(); ++i)
			{
				auto vertexBuffer = mMeshes[0]->getVertexBuffer(i);

				if (updateFixedBuffers || !vertexBuffer->isStatic())
				{
					vertexBuffer->mapBufferData(getVertexCount(numPrimitives));
				}
			}
		}

		mMeshes[0]->setNumPrimitives(numPrimitives);
	}

	ResourcePtr Batch::createMaterial(string const& name, ResourcePtr texture, uint32_t programFlags)
	{
		auto resourceMgr = getResourceManager();
		auto programResource = resourceMgr->getDefault2dProgram(mDefaultVertexShader, mDefaultFragmentShader, mSpecification, programFlags, false, mProgramDescriptor);

		return createMaterial(name, programResource, texture, programFlags);
	}

	ResourcePtr Batch::createMaterial(string const& name, ResourcePtr program, ResourcePtr texture, uint32_t programFlags)
	{
		auto resourceMgr = getResourceManager();

		ProgrammaticMaterialStream* matStream = new ProgrammaticMaterialStream(resourceMgr);

		matStream->setProgram(program->getName());

		matStream->setTexture("TEX1", texture ? texture->getName() : "__mpp_tex_none__");

		auto materialResource = resourceMgr->getResource(name, true);
		if (materialResource)
		{
			materialResource->load();
		}
		else
		{
			materialResource = resourceMgr->declareResource(name, mpp::ResourceStreamPtr(matStream));
			materialResource->load();
		}

		return materialResource;
	}

	BatchVertexAttribute Batch::getColourAttribute() const
	{
		return mColourAttrib;
	}

	size_t Batch::getPrimitiveCount(size_t objectCount) const
	{
		return objectCount;
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void Batch::setSpecificationPointers(Mesh* mesh)
	{
		auto buffers = mesh->getVertexBuffers();

		for (size_t i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto& layout = mSpecification.getVertexBufferAttributeLayout(i);

			for (size_t j = 0; j < layout.getNumAttributes(); ++j)
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

	void Batch::setMinimumCount(size_t count)
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

	bool Batch::usingColour() const
	{
		return mColourAttrib.dataType != mesh::Vertex::DataType::None;
	}
	bool Batch::usingDiffuse() const
	{
		return mUseDiffuse;
	}
}