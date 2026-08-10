#pragma once

#include "mpp/Resource.h"

namespace mpp
{
	class UniformCollection;
	// Shared renderer-facing base for all surface material resources. It is
	// deliberately abstract: concrete assets are BasicMaterial or PbrMaterial.
	class _MPPAPI Material : public Resource
	{
	public:
		enum class ShadingModel
		{
			Basic,
			Pbr
		};

		Material(std::string const& name, std::string const& type, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream)
			: Resource(name, type, renderSystem, resourceMgr, resourceStream) {}
		~Material() override = default;

		virtual ResourcePtr getProgram() = 0;
		virtual int getNumTextures() const = 0;
		virtual void setTexture(int index, ResourcePtr texture) = 0;
		virtual ResourcePtr getTexture(int index) const = 0;
		virtual void setUniforms() = 0;
		virtual ShadingModel getShadingModel() const = 0;
		virtual bool isTransparent() const = 0;
		// Whether the surface has no meaningful back face and must therefore be
		// rasterized with culling off. Only PBR surfaces carry the concept, so this
		// is defaulted rather than pure -- BasicMaterial keeps the model-level flag.
		virtual bool isDoubleSided() const { return false; }
		virtual void validateInstanceUniforms(UniformCollection const& uniforms) const = 0;
	};
}
