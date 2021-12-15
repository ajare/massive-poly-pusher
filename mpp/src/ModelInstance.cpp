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

			MeshInstance* mi = new MeshInstance(mesh, viewPos, modelMatrix, modelCameraProjMatrix, normalMatrix, halfWindowSize, mesh->getPointSize());

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

			MeshInstance* mi = new MeshInstance(mesh, viewPos, modelMatrix, modelCameraProjMatrix, normalMatrix, mesh->getPointSize());
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

			MeshInstance* mi = new MeshInstance(mesh, viewPos, modelMatrix, modelCameraProjMatrix, halfWindowSize, mesh->getPointSize());
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

	void ModelInstance::setParams(shared_ptr<ModelRenderParams> params)
	{
		// Set MeshInstances, etc
		auto const& p = params->getMeshParams();
		auto defaultIt = p.find("");
		
		for (auto meshInstance: mMeshInstances)
		{
			auto& mi = meshInstance.second;
			auto it = p.find(meshInstance.first);
			if (it != p.end())
			{
				mi->render((it->second.flags & ModelRenderParams::Flag_Visible) != 0);
				mi->wireframe((it->second.flags & ModelRenderParams::Flag_Wireframe) != 0);
				
				for (auto const& range: it->second.renderRanges)
				{
					mi->addRenderRange(range.first, range.second);
				}

				mi->setInstanceCount(it->second.instanceCount);
				mi->setPointSize(it->second.pointSize);
				mi->setUniformCollection(it->second.uniforms);

				if (it->second.material)
				{
					mi->setMaterial(it->second.material);
				}
			}
			else if (defaultIt != p.end())
			{
				mi->render((defaultIt->second.flags & ModelRenderParams::Flag_Visible) != 0);
				mi->wireframe((defaultIt->second.flags & ModelRenderParams::Flag_Wireframe) != 0);

				for (auto const& range: defaultIt->second.renderRanges)
				{
					mi->addRenderRange(range.first, range.second);
				}

				mi->setInstanceCount(defaultIt->second.instanceCount);
				mi->setPointSize(defaultIt->second.pointSize);
				mi->setUniformCollection(defaultIt->second.uniforms);

				if (defaultIt->second.material)
				{
					mi->setMaterial(it->second.material);
				}
			}
		}
	}
}