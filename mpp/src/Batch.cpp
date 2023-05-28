#include <cmath>

#include "mpp/Batch.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ProgrammaticModelStream.h"
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
		: mName(name)
		, mDefaultVertexShader(defaultVertexShader)
		, mDefaultFragmentShader(defaultFragmentShader)
		, mProgramDescriptor(descriptor)
		, mCurCount(0)
		, mMaxCount(initialCapacity)
		, mColourAttrib(colourAttrib)
		, mUseDiffuse(useDiffuse)
		, mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
	{
	}

	Batch::~Batch()
	{
		mModel->destroy();
		mMaterial->destroy();
	}

	string const& Batch::getName() const
	{
		return mName;
	}

	ResourcePtr Batch::getModel()
	{
		return mModel;
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

	size_t Batch::getCount() const
	{
		return mCurCount;
	}

	size_t Batch::getCapacity() const
	{
		return mMaxCount;
	}

	ResourcePtr Batch::getTexture()
	{
		return nullptr;
	}

	int Batch::getPointSize() const
	{
		return -1;
	}

	BatchVertexAttribute Batch::getColourAttribute() const
	{
		return mColourAttrib;
	}

	size_t Batch::getPrimitiveCount(size_t objectCount) const
	{
		return objectCount;
	}

	bool Batch::usingColour() const
	{
		return mColourAttrib.dataType != mesh::Vertex::DataType::None;
	}

	bool Batch::usingDiffuse() const
	{
		return mUseDiffuse;
	}

	void Batch::createIndexData(vector<uint8_t>& data, uint32_t start, size_t count)
	{
	}

	void Batch::addIndexedPrimitives(shared_ptr<ProgrammaticModelStream> ms, int meshIndex)
	{
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

	void Batch::create()
	{
		// Create mesh specification
		mSpecification = createMeshSpecification(getPrimitiveType());

		// Create material
		mMaterial = createMaterial(getName() + "_Batch_Material", getTexture(), getProgramFlags());
		mMaterial->load();

		// Create model data
		auto modelStream = make_shared<ProgrammaticModelStream>(mResourceMgr);
		modelStream->setCalculateBounds(false);

		// Create single mesh in model
		auto meshIndex = modelStream->createMesh(getName() + "_Batch_Mesh", mSpecification, mMaterial->getName(), getIndexWidth(), getPointSize());

		auto numVertices = getVertexCount(getCapacity());
		if (numVertices > 0)
		{
			modelStream->addVertexData(meshIndex, VertexData(mSpecification, numVertices));
		}

		if (mSpecification.verticesIndexed())
		{
			addIndexedPrimitives(modelStream, meshIndex);
		}

		// Create and load model
		mModel = mResourceMgr->declareResource(getName() + "_Batch_Model", modelStream);
		mModel->load();

		// Specification pointers
		auto model = static_cast<Model*>(mModel.get());
		setSpecificationPointers(model->getMesh(0));
	}

	void Batch::startUpdate(size_t minimumCount)
	{
		setMinimumCount(minimumCount);
	}

	void Batch::finishUpdate(size_t count, bool updateFixedBuffers)
	{
		auto* mesh = static_cast<Model*>(mModel.get())->getMesh(0);

		mCurCount = count;
		auto numPrimitives = getPrimitiveCount(count);

		if (numPrimitives > 0)
		{
			if (mesh->isIndexed())
			{
				mesh->mapIndexData(numPrimitives);
			}

			for (size_t i = 0; i < mesh->getNumVertexBuffers(); ++i)
			{
				auto vertexBuffer = mesh->getVertexBuffer((int)i);

				if (updateFixedBuffers || !vertexBuffer->isStatic())
				{
					vertexBuffer->mapBufferData(getVertexCount(numPrimitives));
				}
			}
		}

		mesh->setNumPrimitives(numPrimitives);
	}

	ResourcePtr Batch::createMaterial(string const& name, ResourcePtr texture, uint32_t programFlags, bool is2d)
	{
		auto programResource = is2d
			? mResourceMgr->getDefault2dProgram(mDefaultVertexShader, mDefaultFragmentShader, mSpecification, programFlags, false, mProgramDescriptor)
			: mResourceMgr->getDefault3dProgram(mSpecification, programFlags, false, mProgramDescriptor);

		return createMaterial(name, programResource, texture, programFlags);
	}

	ResourcePtr Batch::createMaterial(string const& name, ResourcePtr program, ResourcePtr texture, uint32_t programFlags)
	{
		ProgrammaticMaterialStream* matStream = new ProgrammaticMaterialStream(mResourceMgr);

		matStream->setProgram(program->getName());

		matStream->setTexture("TEX1", texture ? texture->getName() : "__mpp_tex_none__");

		return mResourceMgr->declareResource(name, mpp::ResourceStreamPtr(matStream));
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
		auto mesh = static_cast<Model*>(mModel.get())->getMesh(0);

		if (count > mMaxCount)
		{
			for (size_t i = 0; i < mesh->getNumVertexBuffers(); ++i)
			{
				auto vertexBuffer = mesh->getVertexBuffer((int)i);
				auto& data = vertexBuffer->getBufferData();

				int newSize = getVertexCount(getPrimitiveCount(count)) * vertexBuffer->getVertexStride();
				data.resize(newSize);
			}

			// Index data
			if (indexedVertices())
			{
				createIndexData(mesh->getIndexData(), mMaxCount, count);
			}

			mMaxCount = count;
			setSpecificationPointers(mesh);
		}
	}

}