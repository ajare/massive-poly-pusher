#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include <mpp/MeshInstance.h>

using namespace std;

namespace mpp
{
	/*
	 * Constructor.
	 *
	 */
	MeshInstance::MeshInstance(Mesh const* mesh, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix, glm::vec2 const& halfWindowSize)
		: mwMesh(mesh)
		, mRender(true)
		, mWireframe(false)
		, mBlend(false)
		, mPrimitivesToRender((uint32)-1)
		, mwMaterial(nullptr)
	{
		// Get mcp uniform name
		Material& m = (Material&)(*mesh->getMaterial());
		mwProgram = (Program*)m.getProgram().get();

		mModelCameraProjectionMatrix = modelCameraProjMatrix;
		mLocalTransform = glm::mat4();
		mNormalMatrix = normalMatrix;
		mHalfWindowSize = halfWindowSize;
	}

	/*
	 * Constructor.
	 *
	 */
	MeshInstance::MeshInstance(Mesh const* mesh, glm::mat4 const& modelCameraProjMatrix, glm::mat3 const& normalMatrix)
		: mwMesh(mesh)
	{
		// Get mcp uniform name
		Material& m = (Material&)(*mesh->getMaterial());
		mwProgram = (Program*)m.getProgram().get();

		mModelCameraProjectionMatrix = modelCameraProjMatrix;
		mLocalTransform = glm::mat4();
		mNormalMatrix = normalMatrix;
	}

	/*
	 * Constructor.
	 *
	 */
	MeshInstance::MeshInstance(Mesh const* mesh, glm::mat4 const& modelCameraProjMatrix, glm::vec2 const& halfWindowSize)
		: mwMesh(mesh)
	{
		// Get mcp uniform name
		Material& m = (Material&)(*mesh->getMaterial());
		mwProgram = (Program*)m.getProgram().get();

		mModelCameraProjectionMatrix = modelCameraProjMatrix;
		mLocalTransform = glm::mat4();
		mHalfWindowSize = halfWindowSize;
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
	 * Change material of this particular instance.
	 *
	 */
	void MeshInstance::setMaterial(ResourcePtr material)
	{
		mwMaterial = (Material*)material.get();
		mwProgram = (Program*)mwMaterial->getProgram().get();
	}

	/*
	 * Set a group of uniforms at once.
	 *
	 */
	void MeshInstance::setUniformCollection(UniformCollection const& uniforms)
	{
		mUniforms = uniforms;
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void MeshInstance::setUniform(string const& name, int value)
	{
		mUniforms.setUniform(name, value);
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void MeshInstance::setUniform(string const& name, float value)
	{
		mUniforms.setUniform(name, value);
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void MeshInstance::setUniform(string const& name, glm::vec2 const& value)
	{
		mUniforms.setUniform(name, value);
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void MeshInstance::setUniform(string const& name, glm::vec3 const& value)
	{
		mUniforms.setUniform(name, value);
	}

	/*
	 * Set a uniform value for this mesh instance.
	 *
	 */
	void MeshInstance::setUniform(string const& name, glm::vec4 const& value)
	{
		mUniforms.setUniform(name, value);
	}

	/*
	 * Set number of primitives to render, or -1 to render all, which is default.
	 *
	 */
	void MeshInstance::setRenderCount(uint32 count)
	{
		mPrimitivesToRender = count;
	}

	/*
	 * Upload uniform values for rendering.
	 *
	 */
	void MeshInstance::bindUniforms()
	{
		mUniforms.bindUniforms(mwProgram);

		// Set special uniforms: ModelCameraProjection & normal matrices, half screen size.
		int mcpId = mwProgram->getModelCameraProjectionMatrixId();
		if (mcpId >= 0)
		{
			auto mcpMatrix = mModelCameraProjectionMatrix * mLocalTransform;
			glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(mcpMatrix));
		}

		int normalId = mwProgram->getNormalMatrixId();
		if (normalId >= 0)
		{
			glUniformMatrix3fv(normalId, 1, GL_FALSE, glm::value_ptr(mNormalMatrix));
		}

		int hwsId = mwProgram->getHalfWindowSizeId();
		if (hwsId >= 0)
		{
			glUniform2fv(hwsId, 1, glm::value_ptr(mHalfWindowSize));
		}
	}

	/*
	 * Compares two mesh instances for sorting.
	 *
	 */
	bool MeshInstance::operator <(MeshInstance const* other)
	{
		uint32 progA = this->mwProgram->getId();
		uint32 progB = other->mwProgram->getId();

		if (progA == progB)
		{
			uint32 texA = this->mwMaterial ? this->mwMaterial->getId() : this->mwMesh->getMaterial()->getId();
			uint32 texB = other->mwMaterial ? other->mwMaterial->getId() : other->mwMesh->getMaterial()->getId();

			return texA < texB;
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