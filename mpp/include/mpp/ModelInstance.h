#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include <vector>
#include <string>

#include "mpp/Config.h"
#include "mpp/MeshInstance.h"
#include "mpp/Resource.h"
#include "mpp/Model.h"

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

		void setUniformCollection(UniformCollection const& uniforms);

		void setUniform(std::string const& name, glm::vec3 const& value);

		void setWireframe(bool wireframe);
	};
}

