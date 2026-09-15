#pragma once

#include <vector>
#include <memory>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Resource.h"
#include "mpp/ResourceWrangler.h"
#include "mpp/UniformCollection.h"
#include "mpp/ModelRenderParams.h"

namespace mpp
{
	// One placed instance of a model resource inside a Scene. It acquires the
	// model resource, so its lifetime is bounded by the Scene it belongs to: a
	// SceneModel3d must be destroyed, or removed from its Scene and released by
	// every other owner, before the Batch that declared the model. A pipeline
	// borrows scene models for the duration of one frame and releases them when
	// that frame's graph execution ends, so a SceneModel3d must not be destroyed
	// while the frame that drew it is still executing either. Batch::~Batch
	// reports any outstanding holder by name.
	class _MPPAPI alignas(16) SceneModel3d : public ResourceWrangler
	{
		ResourcePtr mModel;

		std::shared_ptr<ModelRenderParams> mParams;

		std::vector<std::string> mRenderLayers;
		uint64_t mShadowRevision{ 1 };
		// Explicitly classifies this whole model for a graph water pass. Material-
		// based PBR Water classification remains supported independently.
		bool mDeferToWaterPass{ false };

#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::mat4 mTransform{ 1.0f };
#pragma warning(pop)

	public:

		explicit SceneModel3d(ResourcePtr model);
		
		virtual ~SceneModel3d();

		void resetTransform();

		void translate(glm::vec3 const& translate);

		void rotateSelf(float angle, glm::vec3 const& axis);

		void rotateOrigin(float angle, glm::vec3 const& axis);

		void scale(glm::vec3 const& scale);

		void setModel(ResourcePtr model);

		ResourcePtr getModel() const;

		glm::mat4 const& getTransform() const;

		uint64_t getShadowRevision() const;

		// Uses the model's transformed local AABB, conservatively retaining every
		// model whose bounds touch the finite point-light volume. A model with no
		// measured bounds (Model::hasBounds()) is unbounded and always retained.
		bool intersectsSphere(glm::vec3 const& centre, float radius) const;

		std::shared_ptr<ModelRenderParams> getParams();

		void setRenderLayers(std::vector<std::string> layers);

		std::vector<std::string> const& getRenderLayers() const;

		bool isInRenderLayer(std::string const& layer) const;

		void setDeferToWaterPass(bool defer);

		bool getDeferToWaterPass() const;
	};

	typedef std::shared_ptr<SceneModel3d> SceneModel3dPtr;
}