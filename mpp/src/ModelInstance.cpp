#include "mpp/ModelInstance.h"
#include "mpp/Model.h"

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	ModelInstance::ModelInstance(Model const& model, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, glm::vec2 const& halfWindowSize)
	{
		// Iterate over model and create MeshInstances
		for (int i = 0; i < model.getNumMeshes(); ++i)
		{
			Mesh const* mesh = model.getMesh(i);

			MeshInstance* mi = new MeshInstance(mesh, viewPos, modelMatrix, modelCameraProjMatrix, normalMatrix, halfWindowSize);

			string meshName = mesh->getName();
			mMeshInstances[meshName] = mi;
			mOrderedMeshInstances.push_back(mi);
		}
	}

	/*
	* Constructor.
	*
	*/
	ModelInstance::ModelInstance(Model const& model, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix)
	{
		// Iterate over model and create MeshInstances
		for (int i = 0; i < model.getNumMeshes(); ++i)
		{
			Mesh const* mesh = model.getMesh(i);

			MeshInstance* mi = new MeshInstance(mesh, viewPos, modelMatrix, modelCameraProjMatrix, normalMatrix);
			mMeshInstances[mesh->getName()] = mi;
			mOrderedMeshInstances.push_back(mi);
		}
	}

	/*
	 * Constructor.
	 *
	 */
	ModelInstance::ModelInstance(Model const& model, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::vec2 const& halfWindowSize)
	{
		// Iterate over model and create MeshInstances
		for (int i = 0; i < model.getNumMeshes(); ++i)
		{
			Mesh const* mesh = model.getMesh(i);

			MeshInstance* mi = new MeshInstance(mesh, viewPos, modelMatrix, modelCameraProjMatrix, halfWindowSize);
			mMeshInstances[mesh->getName()] = mi;
			mOrderedMeshInstances.push_back(mi);
		}
	}
	
	/*
	 * Destructor
	 *
	 */
	ModelInstance::~ModelInstance()
	{
		for (auto mesh: mMeshInstances)
		{
			delete mesh.second;
		}
	}

	/*
	 * Get all mesh instances.
	 *
	 */
	vector<MeshInstance*>& ModelInstance::getMeshInstances()
	{
		return mOrderedMeshInstances;
	}

	/*
	 * Get all mesh instances.
	 *
	 */
	vector<MeshInstance*> const& ModelInstance::getMeshInstances() const
	{
		return mOrderedMeshInstances;
	}

	/*
	 * Check whether the named instance exists.
	 *
	 */
	bool ModelInstance::hasMeshInstance(string const& name) const
	{
		return mMeshInstances.find(name) != mMeshInstances.end();
	}

	/*
	 * Get the named mesh instance.
	 *
	 */
	MeshInstance* ModelInstance::getMeshInstance(string const& name)
	{
		return mMeshInstances[name];
	}

	/*
	 * Sets uniforms for this model.
	 *
	 */
	void ModelInstance::setUniformCollection(UniformCollection const& uniforms)
	{
		for (auto meshInstance: mMeshInstances)
		{
			meshInstance.second->setUniformCollection(uniforms);
		}
	}

	/*
	 * Render as wireframe.
	 *
	 */
	void ModelInstance::setWireframe(bool wireframe)
	{
		for (auto meshInstance: mMeshInstances)
		{
			meshInstance.second->wireframe(wireframe);
		}
	}

	void ModelInstance::setInstanceCount(size_t instanceCount)
	{
		for (auto meshInstance: mMeshInstances)
		{
			meshInstance.second->setInstanceCount(instanceCount);
		}
	}

	void ModelInstance::setUniform(string const& name, int32_t value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::setUniform(string const& name, uint32_t value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::setUniform(string const& name, float value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::setUniform(string const& name, glm::vec2 const& value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::setUniform(string const& name, glm::vec3 const& value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::setUniform(string const& name, glm::vec4 const& value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::setUniform(string const& name, size_t count, int32_t const* values)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, count, values);
		}
	}

	void ModelInstance::setUniform(string const& name, size_t count, uint32_t const* values)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, count, values);
		}
	}

	void ModelInstance::setUniform(string const& name, size_t count, float const* values)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, count, values);
		}
	}

	void ModelInstance::setUniform(string const& name, program::GLSLType type, size_t count, char const* data)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, type, count, data);
		}
	}

	void ModelInstance::updateUniform(string const& name, int32_t value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::updateUniform(string const& name, uint32_t value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::updateUniform(string const& name, float value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::updateUniform(string const& name, glm::vec2 const& value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::updateUniform(string const& name, glm::vec3 const& value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::updateUniform(string const& name, glm::vec4 const& value)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, value);
		}
	}

	void ModelInstance::updateUniform(string const& name, size_t count, int32_t const* values)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, count, values);
		}
	}

	void ModelInstance::updateUniform(string const& name, size_t count, uint32_t const* values)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, count, values);
		}
	}

	void ModelInstance::updateUniform(string const& name, size_t count, float const* values)
	{
		for (auto meshInstance : mMeshInstances)
		{
			meshInstance.second->getUniformCollection().setUniform(name, count, values);
		}
	}
}