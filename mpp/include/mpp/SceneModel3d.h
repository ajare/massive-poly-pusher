#pragma once

#include <vector>
#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/UniformCollection.h"

namespace mpp
{
	class _MPPAPI __declspec(align(16)) SceneModel3d
	{
		ResourcePtr mModel;

		bool mWireframe;

		bool mVisible;

		size_t mInstanceCount;

		std::shared_ptr<UniformCollection> mUniforms;

#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::mat4 mTransform;
#pragma warning(pop)

	public:

		explicit SceneModel3d(ResourcePtr model, bool hasUniforms = false);
		
		virtual ~SceneModel3d();

		void translate(glm::vec3 const& translate);

		void rotateSelf(float angle, glm::vec3 const& axis);

		void rotateOrigin(float angle, glm::vec3 const& axis);

		void scale(glm::vec3 const& scale);

		ResourcePtr getModel() const;

		glm::mat4 const& getTransform() const;

		std::shared_ptr<UniformCollection> getUniformCollection();

		void setWireframe(bool wireframe);

		bool isWireframe() const;

		void setVisible(bool visible);

		bool isVisible() const;

		void setInstanceCount(size_t instanceCount);

		size_t getInstanceCount() const;
	};

	typedef std::shared_ptr<SceneModel3d> SceneModel3dPtr;
}