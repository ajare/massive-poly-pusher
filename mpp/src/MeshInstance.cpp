#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include "mpp/MeshInstance.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	MeshInstance::MeshInstance()
		: ResourceWrangler("MeshInstance")
		, mwMesh(nullptr)
		, mRender(true)
		, mWireframe(false)
		, mBlend(false)
		, mInstanceCount(0)
	{
	}

	MeshInstance::~MeshInstance()
	{
		teardown();
	}

	void MeshInstance::commonSetup(Mesh const* mesh)
	{
		teardown();

		mwMesh = mesh;
		mRender = true;
		mWireframe = false;
		mBlend = false;
		mInstanceCount = 1;

		mMaterial = mesh->getMaterial();
		mMaterial->acquire(this);
	}

	void MeshInstance::setup(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, glm::vec2 const& halfWindowSize, float pointSize)
	{
		commonSetup(mesh);

		mViewPos = viewPos;
		mModelMatrix = modelMatrix;
		mModelCameraProjectionMatrix = modelCameraProjMatrix;
		mLocalTransform = glm::mat4();
		mNormalMatrix = normalMatrix;
		mHalfWindowSize = halfWindowSize;
		mPointSize = pointSize;
	}

	void MeshInstance::setup(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, float pointSize)
	{
		commonSetup(mesh);

		mViewPos = viewPos;
		mModelMatrix = modelMatrix;
		mModelCameraProjectionMatrix = modelCameraProjMatrix;
		mLocalTransform = glm::mat4();
		mNormalMatrix = normalMatrix;
		mPointSize = pointSize;
	}

	void MeshInstance::setup(Mesh const* mesh, glm::vec3 const& viewPos, glm::mat4 const& modelMatrix, glm::mat4 const& modelCameraProjMatrix, glm::vec2 const& halfWindowSize, float pointSize)
	{
		commonSetup(mesh);

		mViewPos = viewPos;
		mModelMatrix = modelMatrix;
		mModelCameraProjectionMatrix = modelCameraProjMatrix;
		mLocalTransform = glm::mat4();
		mHalfWindowSize = halfWindowSize;
		mPointSize = pointSize;
	}

	void MeshInstance::teardown()
	{
		mwMesh = nullptr;
		mRender = true;
		mWireframe = false;
		mBlend = false;
		mInstanceCount = 0;

		if (mMaterial)
		{
			mMaterial->release(this);
			mMaterial.reset();
		}

		mRenderRanges.clear();
		mTextureOverrides.clear();

		if (mUniforms)
		{
			mUniforms.reset();
		}
	}

	/*
	 * Enable/disable rendering of this particular mesh.
	 *
	 */
	void MeshInstance::render(bool render)
	{
		mRender = render;
	}

	/*
	 * Should this mesh be rendered?
	 *
	 */
	bool MeshInstance::render() const
	{
		return mRender;
	}

	/*
	 * Render as wireframe.
	 *
	 */
	void MeshInstance::wireframe(bool wireframe)
	{
		mWireframe = wireframe;
	}

	/*
	 * Should this mesh be rendered as wireframe?
	 *
	 */
	bool MeshInstance::wireframe() const
	{
		return mWireframe;
	}

	/*
	 * Render with blending.
	 *
	 */
	void MeshInstance::blend(bool blend)
	{
		mBlend = blend;
	}

	/*
	 * Should this mesh be rendered with blending?
	 *
	*/	
	bool MeshInstance::blend() const
	{
		return mBlend;
	}

	/*
	 * Set point size (only has effect with GL_POINTS).
	 *
	 */
	void MeshInstance::setPointSize(float pointSize)
	{
		mPointSize = pointSize;
	}

	/*
	 * Get point size.
	 *
	 */
	float MeshInstance::getPointSize() const
	{
		return mPointSize;
	}

	void MeshInstance::setInstanceCount(size_t instanceCount)
	{
		mInstanceCount = instanceCount;
	}

	size_t MeshInstance::getInstanceCount() const
	{
		return mInstanceCount;
	}

	/*
	 * Change material of this particular instance.
	 *
	 */
	void MeshInstance::setMaterial(ResourcePtr material)
	{
		if (mMaterial)
		{
			mMaterial->release(this);
		}

		mMaterial = material;
	
		if (mMaterial)
		{
			mMaterial->acquire(this);
		}
	}

	/*
	 * Get material of this particular instance.
	 *
	 */
	ResourcePtr MeshInstance::getMaterial()
	{
		return mMaterial;
	}

	/*
	 * Set texture
	 *
	 */
	void MeshInstance::setTexture(int index, ResourcePtr texture)
	{
		if (index >= (int)mTextureOverrides.size())
		{
			mTextureOverrides.resize(index + 1);
		}

		mTextureOverrides[index] = texture;
	}

	/*
	 * Get texture (or override)
	 *
	 */
	ResourcePtr MeshInstance::getTexture(int texture)
	{
		if (mTextureOverrides.size() > (size_t)texture && mTextureOverrides[texture])
		{
			return mTextureOverrides[texture];
		}
		else
		{
			return static_cast<Material*>(mMaterial.get())->getTexture(texture);
		}
	}

	/*
	 * Set a group of uniforms at once.
	 *
	 */
	void MeshInstance::setUniformCollection(shared_ptr<UniformCollection> uniforms)
	{
		mUniforms = uniforms;
	}

	shared_ptr<UniformCollection> MeshInstance::getUniformCollection()
	{
		return mUniforms;
	}

	/*
	 * Set number of primitives to render.
	 *
	 */
	void MeshInstance::setRenderCount(uint32_t count)
	{
		mRenderRanges.clear();
		mRenderRanges.push_back(make_pair(0, count));
	}

	void MeshInstance::addRenderRange(uint32_t start, size_t count)
	{
		mRenderRanges.push_back(make_pair(start, count));
	}

	/*
	 * Upload uniform values for rendering.
	 *
	 */
	void MeshInstance::bindUniforms()
	{
		auto program = static_cast<Material*>(mMaterial.get())->getProgram();

		if (mUniforms)
		{
			mUniforms->bindUniforms(program);
		}

		// Set special uniforms: Model, ModelCameraProjection & Normal matrices, half screen size.
		Program* p = static_cast<Program*>(program.get());

		int vpId = p->getViewPosId();
		if (vpId >= 0)
		{
			GL_CHECK(glUniform3fv(vpId, 1, glm::value_ptr(mViewPos)));
		}

		int mId = p->getModelMatrixId();
		if (mId >= 0)
		{
			auto mMatrix = mModelMatrix * mLocalTransform;
			GL_CHECK(glUniformMatrix4fv(mId, 1, GL_FALSE, glm::value_ptr(mMatrix)));
		}

		int mcpId = p->getModelCameraProjectionMatrixId();
		if (mcpId >= 0)
		{
			auto mcpMatrix = mModelCameraProjectionMatrix * mLocalTransform;
			GL_CHECK(glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(mcpMatrix)));
		}

		int normalId = p->getNormalMatrixId();
		if (normalId >= 0)
		{
			GL_CHECK(glUniformMatrix3fv(normalId, 1, GL_FALSE, glm::value_ptr(mNormalMatrix)));
		}

		int hwsId = p->getHalfWindowSizeId();
		if (hwsId >= 0)
		{
			GL_CHECK(glUniform2fv(hwsId, 1, glm::value_ptr(mHalfWindowSize)));
		}

		int psId = p->getPointSizeId();
		if (psId >= 0)
		{
			GL_CHECK(glUniform1f(psId, mPointSize));
		}
	}

	/*
	 * Compares two mesh instances for sorting.
	 *
	 */
	bool MeshInstance::operator <(MeshInstance const* other)
	{
		auto m1 = static_cast<Material*>(mMaterial.get());
		auto program1 = m1->getProgram();
		auto p1 = static_cast<Program*>(program1.get()); 

		auto m2 = static_cast<Material*>(other->mMaterial.get());
		auto program2 = m2->getProgram();
		auto p2 = static_cast<Program*>(program2.get());

		uint32_t progA = p1->getId();
		uint32_t progB = p2->getId();

		if (progA == progB)
		{
			return m1->getId() < m2->getId();
		}
		else
		{
			// Just sort by program by now
			return progA < progB;
		}
	}

	/*
	 * Translate mesh locally.
	 *
	 */
	void MeshInstance::translate(glm::vec3 const& translate)
	{
		mLocalTransform = glm::translate(mLocalTransform, translate);
	}

	/*
	 * Scale mesh locally.
	 *
	 */
	void MeshInstance::scale(glm::vec3 const& scale)
	{
		mLocalTransform = glm::scale(mLocalTransform, scale);
	}

	/*
	 * Rotate mesh locally.
	 *
	 */
	void MeshInstance::rotate(glm::vec3 const& axis, float angle)
	{
		mLocalTransform = glm::rotate(mLocalTransform, glm::radians(angle), axis);
		mNormalMatrix *= glm::mat3(mLocalTransform);
	}
}