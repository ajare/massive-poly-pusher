#pragma once

#include <cstdlib>

#include <glm/gtc/matrix_transform.hpp>

#include "mpp/Config.h"
#include "mpp/Material.h"
#include "mpp/Mesh.h"
#include "mpp/UniformCollection.h"

namespace mpp
{
	class _MPPAPI __declspec(align(16)) MeshInstance
	{
		friend class RenderSystem;

	private:

		Mesh const* mwMesh;

		bool mRender;

		bool mWireframe;

		bool mBlend;

		float mPointSize;

		size_t mInstanceCount;

		uint32_t mPrimitivesToRender;

		ResourcePtr mMaterial;

		UniformCollection mUniforms;

#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::vec3 mViewPos;
		alignas(16) glm::mat4 mModelMatrix;
		alignas(16) glm::mat4 mModelCameraProjectionMatrix;
		alignas(16) glm::mat4 mLocalTransform;
		alignas(16) glm::mat3 mNormalMatrix;
		alignas(16) glm::vec2 mHalfWindowSize;
#pragma warning(pop)

	public:

		MeshInstance(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, glm::vec2 const& halfWindowSize);

		MeshInstance(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix);

		MeshInstance(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::vec2 const& halfWindowSize);

		bool operator <(MeshInstance const* other);

		void render(bool render);

		bool render() const;

		void wireframe(bool wireframe);

		bool wireframe() const;

		void blend(bool blend);

		bool blend() const;

		void setPointSize(float pointSize);

		float getPointSize() const;

		void setInstanceCount(size_t instanceCount);

		size_t getInstanceCount() const;

		void setMaterial(ResourcePtr material);

		void setUniformCollection(UniformCollection const& uniforms);

		UniformCollection& getUniformCollection();

		void setRenderCount(uint32_t count);

		void translate(glm::vec3 const& translate);

		void scale(glm::vec3 const& scale);

		void rotate(glm::vec3 const& axis, float angle);

		void bindUniforms();

		// Override new/delete to force alignment on 16-byte boundary.
		void* operator new(size_t size)
		{
			return _aligned_malloc(size, 16);
		}

		void operator delete(void* p)
		{
			_aligned_free(p);
		}
	};
}

