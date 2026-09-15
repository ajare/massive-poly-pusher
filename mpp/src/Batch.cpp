#include <cmath>
#include <format>

#include "mpp/Batch.h"
#include "mpp/ProgrammaticBasicMaterialStream.h"
#include "mpp/ProgrammaticModelStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/StaticLogger.h"

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
		: ResourceWrangler(name)
		, mName(name)
		, mDefaultVertexShader(defaultVertexShader)
		, mDefaultFragmentShader(defaultFragmentShader)
		, mProgramDescriptor(descriptor)
		, mColourAttrib(colourAttrib)
		, mInitialCapacity(initialCapacity)
		, mUseDiffuse(useDiffuse)
		, mRenderSystem(renderSystem)
		, mResourceMgr(resourceMgr)
	{
	}

	Batch::~Batch()
	{
		destroy();

		// Remove the Model and Material this batch declared. A resource that is
		// still held by another wrangler cannot be deleted without destroying
		// something still in use, so deleteDeclaredResource reports the holders
		// instead of skipping the delete silently.
		deleteDeclaredResource(mModel, "Model");
		deleteDeclaredResource(mMaterial, "Material");
	}

	string const& Batch::getName() const
	{
		return mName;
	}

	ResourcePtr Batch::getModel()
	{
		return mModel;
	}

	ResourcePtr Batch::getMaterial()
	{
		return mMaterial;
	}

	mesh::MeshSpecification const& Batch::getSpecification() const
	{
		return mSpecification;
	}

	const pair<char*, size_t>& Batch::getAttributeData(uint32_t meshIndex, string const& name) const
	{
		auto it = mMeshes[meshIndex].dataPointers.find(name);
		if (it == mMeshes[meshIndex].dataPointers.end())
		{
			return msNonExistentAttribute;
		}

		return mMeshes[meshIndex].dataPointers.at(name);
	}

	size_t Batch::getCount(uint32_t meshIndex) const
	{
		return mMeshes[meshIndex].curCount;
	}

	size_t Batch::getCapacity(uint32_t meshIndex) const
	{
		return mMeshes[meshIndex].maxCount;
	}

	ResourcePtr Batch::getTexture()
	{
		return nullptr;
	}

	float Batch::getPointSize() const
	{
		return -1.0;
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

		auto vb = mesh->createVertexBuffer(vertexCount, bufferSize, !staticData, staticData, dataPtr);

		for (size_t j = 0; j < layout.getNumAttributes(); ++j)
		{
			auto& attrib = layout.getAttribute(j);
			vb->setAttribute(
				attrib.attributeId,
				attrib.dataType,
				mesh::Vertex::getComponentSize(attrib.component),
				(int)attrib.offsetInBytes,
				attrib.normalised);
		}
	}

	shared_ptr<ModelStream> Batch::createModelStream()
	{
		auto modelStream = make_shared<ProgrammaticModelStream>(mResourceMgr);
		modelStream->setCalculateBounds(false);

		// Create single mesh in model
		auto meshIndex = modelStream->createMesh(getName() + "_Batch_Mesh", getSpecification(), mMaterial->getName(), getIndexWidth(), getPointSize());

		auto numVertices = getVertexCount(mInitialCapacity);

		if (numVertices > 0)
		{
			modelStream->addVertexData(meshIndex, VertexData(getSpecification(), numVertices));
		}

		if (mSpecification.verticesIndexed())
		{
			addIndexedPrimitives(modelStream, (int)meshIndex);
		}

		return modelStream;
	}

	void Batch::create()
	{
		// Create mesh specification
		mSpecification = createMeshSpecification(getPrimitiveType());

		// Create material
		mMaterial = createMaterial(getName() + "_Batch_Material", getTexture(), getProgramFlags());
		mMaterial->acquire(this);
		mMaterial->load();

		// Create model data
		auto modelStream = createModelStream();

		// Create and load model
		mModel = mResourceMgr->declareResource(getName() + "_Batch_Model", modelStream).first;
		mModel->acquire(this);
		mModel->load();

		// Create mesh data
		auto model = static_pointer_cast<Model>(mModel);

		for (int i = 0; i < model->getNumMeshes(); ++i)
		{
			mMeshes.push_back({
				{},
				0,
				mInitialCapacity
			});

			// Specification pointers
			setSpecificationPointers((uint32_t)i, model->getMesh(i));
		}
	}

	void Batch::destroy()
	{
		if (mModel) mModel->release(this);
		if (mMaterial) mMaterial->release(this);
	}

	void Batch::deleteDeclaredResource(ResourcePtr const& resource, string const& role)
	{
		if (!resource)
		{
			return;
		}

		if (!resource->getRefCount())
		{
			mResourceMgr->deleteResource(resource->getName());
			return;
		}

		// The resource is still in use, so its name registration stays. Say so
		// loudly: the alternative is a stranded resource whose name the next
		// Batch of the same name cannot redeclare.
		auto const message = format(
			"Batch '{}' destroyed while its {} '{}' is still referenced ({}). "
			"The name remains registered, so a later resource of the same name will fail to declare; "
			"every user of this Batch's resources, normally the SceneModel3d that wraps its Model, must be destroyed before the Batch.",
			getName(), role, resource->getName(), describeOutstandingResourceReferences(*resource));

		// This runs from a destructor, which must not throw, and the static log
		// opens a file. A failure there must not suppress the report below.
		try
		{
			static_log_message(MPP_RESOURCE_LOGFILE, message);
		}
		catch (...)
		{
		}

		mResourceMgr->errorMessage(message);
	}

	void Batch::startUpdate(size_t minimumCount, size_t vertexCount)
	{
		setMinimumCount(minimumCount, vertexCount);
	}

	void Batch::finishUpdate(size_t primitiveCount, size_t vertexCount, bool updateFixedBuffers)
	{
		for (uint32_t meshIndex = 0; meshIndex < (uint32_t)mMeshes.size(); ++meshIndex)
		{
			auto* mesh = static_cast<Model*>(mModel.get())->getMesh(meshIndex);

			mMeshes[meshIndex].curCount = primitiveCount;
			auto numPrimitives = getPrimitiveCount(primitiveCount);

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
						vertexBuffer->mapBufferData(vertexCount);
					}
				}
			}

			mesh->setNumPrimitives(numPrimitives);
		}
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
		ProgrammaticBasicMaterialStream* matStream = new ProgrammaticBasicMaterialStream(mResourceMgr);

		matStream->setProgram(program->getName());

		matStream->setTexture("TEX1", texture ? texture->getName() : "__mpp_tex_none__");

		return mResourceMgr->declareResource(name, mpp::ResourceStreamPtr(matStream)).first;
	}

	/*
	 * Set the data pointers for the mesh specification.
	 *
	 */
	void Batch::setSpecificationPointers(uint32_t meshIndex, Mesh* mesh)
	{
		auto buffers = mesh->getVertexBuffers();

		for (size_t i = 0; i < mSpecification.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto& layout = mSpecification.getVertexBufferAttributeLayout((uint32_t)i);

			for (size_t j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto& attrib = layout.getAttribute(j);
				if (buffers[i]->getBufferData().size() > 0)
				{
					auto dataPtr = (char*)&((buffers[i]->getBufferData()[0])) + (int)attrib.offsetInBytes;
					mMeshes[meshIndex].dataPointers[attrib.identifier] = make_pair(dataPtr, layout.getVertexSize());
				}
			}
		}
	}

	void Batch::setMinimumCount(size_t count, size_t vertexCount)
	{
		for (uint32_t meshIndex = 0; meshIndex < (uint32_t)mMeshes.size(); ++meshIndex)
		{
			auto mesh = static_cast<Model*>(mModel.get())->getMesh(meshIndex);

			if (count > mMeshes[meshIndex].maxCount)
			{
				for (size_t i = 0; i < mesh->getNumVertexBuffers(); ++i)
				{
					auto vertexBuffer = mesh->getVertexBuffer((int)i);
					auto& data = vertexBuffer->getBufferData();

					int newSize = (int)(vertexCount * vertexBuffer->getVertexStride());
					data.resize(newSize);
				}

				// Index data
				if (indexedVertices())
				{
					createIndexData(mesh->getIndexData(), (uint32_t)mMeshes[meshIndex].maxCount, count);
				}

				mMeshes[meshIndex].maxCount = count;
				setSpecificationPointers(meshIndex, mesh);
			}
		}
	}
}