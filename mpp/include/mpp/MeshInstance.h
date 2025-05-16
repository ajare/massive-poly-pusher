#pragma once

#include <cstdlib>
#include <memory>
#include <array>

#include <glm/gtc/matrix_transform.hpp>

#include "mpp/Config.h"
#include "mpp/ResourceWrangler.h"
#include "mpp/Material.h"
#include "mpp/Mesh.h"
#include "mpp/UniformCollection.h"
#include "mpp/VertexBufferRenderCommand.h"

namespace mpp
{
	class _MPPAPI __declspec(align(16)) MeshInstance : public ResourceWrangler
	{
		friend class RenderSystem;

	private:

		Mesh const* mwMesh;

		bool mRender;

		bool mWireframe;

		bool mBlend;

		float mPointSize;

		size_t mInstanceCount;

		std::vector<VertexBufferRenderCommand> mRenderCommands;

		ResourcePtr mMaterial;

		std::array<ResourcePtr, 2> mTextureOverrides;

		std::shared_ptr<UniformCollection> mUniforms;

#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::vec3 mViewPos;
		alignas(16) glm::mat4 mModelMatrix;
		alignas(16) glm::mat4 mModelCameraProjectionMatrix;
		alignas(16) glm::mat4 mLocalTransform;
		alignas(16) glm::mat3 mNormalMatrix;
		alignas(16) glm::vec2 mHalfWindowSize;
#pragma warning(pop)

	private:

		void commonSetup(Mesh const* mesh);

		void teardown();

	public:

		MeshInstance();

		virtual ~MeshInstance();

		bool operator <(MeshInstance const* other);

		void setup(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, glm::vec2 const& halfWindowSize, float pointSize);

		void setup(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, float pointSize);

		void setup(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::vec2 const& halfWindowSize, float pointSize);

		void release();

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

		ResourcePtr getMaterial();

		void setTexture(int index, ResourcePtr texture);

		ResourcePtr getTexture(int texture);

		void setUniformCollection(std::shared_ptr<UniformCollection> uniforms);

		std::shared_ptr<UniformCollection> getUniformCollection();

		void setRenderCount(uint32_t count);

		void addRenderCommand(VertexBufferRenderCommand const& renderCmd);

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

