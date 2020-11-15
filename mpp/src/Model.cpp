#include "mpp/Config.h"

#if MPP_PLATFORM == MPP_PLATFORM_WIN32
#include <Windows.h>
#endif

#include <cassert>
#include <glew/glew.h>
#include <gl/gl.h>

#include "utils/MemTracker.h"

#include "mpp/Model.h"
#include "mpp/ModelStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{
	using namespace mesh;

	/*
	 * Constructor.
	 *
	 */
	Model::Model(string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
		: Resource(name, "Model", renderSystem, resourceMgr, resourceStream)
	{
	}

	/*
	 * Destructor.
	 *
	 */
	Model::~Model()
	{
		for (auto mesh: mMeshes)
		{
			delete mesh;
		}
	}

	/*
	 * Create the data required.
	 *
	 */
	void Model::createImpl()
	{
		ModelStream* mStr = dynamic_cast<ModelStream*>(getResourceStream().get());
		if (!mStr)
		{
			THROW_MPP("Could not cast to type 'ModelStream'", __LINE__, __FILE__, __func__);
		}

		auto resourceMgr = getResourceManager();

		for (size_t i = 0; i < mStr->getNumMeshDefinitions(); ++i)
		{
			MeshDefinition* meshDef = mStr->getMeshDefinition(i);

			// If it's an MPP model, prepend name, as the material will be specific to it.
			string materialName = mStr->markUpMaterialName(getName(), meshDef->getMaterial());
			
			ResourcePtr	material = resourceMgr->getResource(materialName);
			material->load();

			// Don't check vertex attribute mapping for internal resources as they may not
			// actually have a mapping yet.
			if (!utils::StringUtils::startsWith(getName(), "__mpp") &&
				!checkVertexAttributeMapping(material, meshDef))
			{
				THROW_MPP(
					utils::StringUtils::format("Vertex attribute mismatch between material '{}' and mesh '{}' of model '{}'.",
						material->getName(), meshDef->getName(), getName()),
					__LINE__, __FILE__, __func__);
			}

			Mesh* mesh = nullptr;

			// Get index data and convert as required
			auto storageType = meshDef->getStorageType();
			auto primitiveType = meshDef->getPrimitiveType();
			int primitiveSize = mesh::Primitive::size(primitiveType);
			int primitiveCount = meshDef->getNumPrimitives();
			float pointSize = meshDef->getPointSize();

			if (meshDef->isIndexed())
			{
				int indexWidth = meshDef->getIndexWidth();
				int indexWidthBytes = indexWidth / 8;
				int indexSize = primitiveCount * primitiveSize * indexWidthBytes;

				auto indexDataPtr = (uint8_t*)meshDef->getIndexData().get();

				vector<uint8_t> indexData;
				for (int i = 0; i < indexSize; ++i)
				{
					indexData.push_back(*indexDataPtr++);
				}

				mesh = new Mesh(getRenderSystem(), meshDef->getName(), material, primitiveType, primitiveCount, indexWidth, indexData, storageType, pointSize);
			}
			else
			{
				mesh = new Mesh(getRenderSystem(), meshDef->getName(), material, primitiveType, primitiveCount, storageType, pointSize);
			}

			for (int j = 0; j < meshDef->getNumVertexBufferDefinitions(); ++j)
			{
				VertexBufferDefinition const* bufferDef = meshDef->getVertexBufferDefinition(j);

				VertexBuffer* buffer = mesh->createVertexBuffer(
					bufferDef->getVertexCount(),
					bufferDef->getVertexStride(),
					bufferDef->getStreaming(),
					false,
					bufferDef->getData());

				for (int k = 0; k < bufferDef->getNumAttributes(); ++k)
				{
					auto const& attrib = bufferDef->getAttribute(k);
					buffer->setAttribute(
						attrib.attributeId,
						attrib.dataType,
						Vertex::getComponentSize(attrib.component),
						attrib.offsetInBytes,
						attrib.normalised);
				}
			}

			mMeshes.push_back(mesh);
		}
	}

	/*
	 * Destroy the model data.
	 *
	 */
	void Model::destroyImpl()
	{
		for (auto it: mMeshes)
		{
			delete it;
		}
		
		mMeshes.clear();
	}

	/*
	 * Create OpenGL model.
	 *
	 */
	void Model::loadImpl()
	{
		for (auto it: mMeshes)
		{
			it->load();
		}
	}

	/*
	 * Destroy the OpenGL model.
	 *
	 */
	void Model::unloadImpl()
	{
		for (auto it: mMeshes)
		{
			it->unload();
		}
	}

	/*
	 * Check that the material's program vertex attributes match the mesh's.
	 *
	 */
	bool Model::checkVertexAttributeMapping(ResourcePtr material, MeshDefinition* meshDef)
	{
		// Get mesh attributes
		vector<size_t> meshComponentSizes;
		for (int i = 0; i < meshDef->getNumVertexBufferDefinitions(); ++i)
		{
			auto vbDef = meshDef->getVertexBufferDefinition(i);
			for (int j = 0; j < vbDef->getNumAttributes(); ++j)
			{
				auto attrib = vbDef->getAttribute(j);
				meshComponentSizes.push_back(Vertex::getComponentSize(attrib.component));
			}
		}

		// Compare against program attributes
		auto programRes = static_cast<Material*>(material.get())->getProgram();
		auto program = static_cast<Program*>(programRes.get());

		auto const& programAttrs = program->getVertexAttributes();
		if (meshComponentSizes.size() != programAttrs.size())
		{
			return false;
		}

		for (size_t i = 0; i < programAttrs.size(); ++i)
		{
			if (programAttrs[i].numComponents != meshComponentSizes[i])
			{
				return false;
			}
		}

		return true;
	}

	/*
	 * Get the number of triangles in the model
	 *
	 */
	int Model::getNumTriangles() const
	{
		int tris = 0;
		for (auto it : mMeshes)
		{
			tris += it->getNumPrimitives();
		}

		return tris;
	}

	/*
	 * Get the number of meshes in the model
	 *
	 */
	int Model::getNumMeshes() const
	{
		return (int)mMeshes.size();
	}

	/*
	 * Get the indexed mesh.
	 *
	 */
	Mesh const* Model::getMesh(int index) const
	{
		assert(index >= 0 && "Model::getMesh() 'index' argument out of range!");
		return mMeshes[index];
	}

	/*
	 * Get the indexed mesh.
	 *
	 */
	Mesh* Model::getMesh(int index)
	{
		assert(index >= 0 && "Model::getMesh() 'index' argument out of range!");
		return mMeshes[index];
	}

	/*
	 * Set meshes to be dynamically updatable, overriding current settings.  The meshes will have to
	 * be reloaded if they are currently loaded.
	 */
	void Model::setMeshesDynamic()
	{
		for (auto const mesh: mMeshes)
		{
			mesh->setStorageType(mesh::VertexBufferStorageType::Dynamic);
		}
	}

	/*
	 * Set meshes to be static, overriding current settings.  The meshes will have to
	 * be reloaded if they are currently loaded.
	 */
	void Model::setMeshesStatic()
	{
		for (auto const mesh: mMeshes)
		{
			mesh->setStorageType(mesh::VertexBufferStorageType::Static);
		}
	}
}