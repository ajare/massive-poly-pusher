#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include <memory>
#include <vector>
#include <string>

#include "mpp/Config.h"
#include "mpp/MeshInstance.h"
#include "mpp/Resource.h"
#include "mpp/Model.h"
#include "mpp/ModelRenderParams.h"

namespace mpp
{
	class _MPPAPI ModelInstance
	{
		std::map<std::string, MeshInstance*> mMeshInstances;

		std::vector<MeshInstance*> mOrderedMeshInstances;

	public:

		ModelInstance(Model const& model, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, glm::vec2 const& halfWindowSize);

		ModelInstance(Model const& model, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix);

		ModelInstance(Model const& model, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::vec2 const& halfWindowSize);

		~ModelInstance();

		std::vector<MeshInstance*>& getMeshInstances();

		std::vector<MeshInstance*> const& getMeshInstances() const;

		bool hasMeshInstance(std::string const& name) const;

		MeshInstance* getMeshInstance(std::string const& name);

		void setParams(std::shared_ptr<ModelRenderParams> params);

	};
}

