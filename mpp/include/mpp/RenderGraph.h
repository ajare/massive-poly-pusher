#pragma once

#include <cstdint>
#include <string>
#include <vector>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/TextureParams.h"
#include "mpp/UniformCollection.h"

namespace mpp
{
	struct Caps;

	enum class GraphImageFormat
	{
		Rgba8,
		Rgba16f,
		Rg16f,
		Depth24,
		Depth24Stencil8
	};

	enum class GraphImageUsage : uint32_t
	{
		None = 0,
		Sampled = 1 << 0,
		ColourAttachment = 1 << 1,
		DepthAttachment = 1 << 2,
		Presentation = 1 << 3
	};

	inline GraphImageUsage operator |(GraphImageUsage left, GraphImageUsage right)
	{
		return (GraphImageUsage)((uint32_t)left | (uint32_t)right);
	}

	inline bool hasGraphImageUsage(GraphImageUsage value, GraphImageUsage test)
	{
		return ((uint32_t)value & (uint32_t)test) != 0;
	}

	struct _MPPAPI GraphImageDesc
	{
		GraphImageFormat format{ GraphImageFormat::Rgba8 };
		glm::uvec2 absoluteSize{ 0 };
		glm::vec2 relativeSize{ 1.0f };
		uint32_t samples{ 1 };
		uint32_t mipLevels{ 1 };
		GraphImageUsage usage{ GraphImageUsage::None };
		TextureParams params;
		TextureColourSpace colourSpace{ TextureColourSpace::Linear };
		bool transient{ true };
		bool external{ false };
	};

	struct _MPPAPI GraphImageInfo
	{
		std::string name;
		GraphImageDesc desc;
		std::string importName;
	};

	struct _MPPAPI GraphImageHandle	{
		uint32_t id{ UINT32_MAX };
		uint32_t version{ 0 };

		bool isValid() const { return id != UINT32_MAX; }
	};

	struct _MPPAPI GraphPassHandle
	{
		uint32_t id{ UINT32_MAX };
		bool isValid() const { return id != UINT32_MAX; }
	};

	enum class GraphPassType { Scene, Fullscreen, Present };
	enum class GraphLoadOp { Load, Clear, DontCare };
	enum class GraphStoreOp { Store, DontCare };

	struct _MPPAPI GraphColourOutput
	{
		GraphImageHandle image;
		GraphLoadOp load{ GraphLoadOp::DontCare };
		GraphStoreOp store{ GraphStoreOp::Store };
		glm::vec4 clearColour{ 0.0f };
	};

	struct _MPPAPI GraphDepthOutput
	{
		GraphImageHandle image;
		GraphLoadOp load{ GraphLoadOp::DontCare };
		GraphStoreOp store{ GraphStoreOp::Store };
		float clearDepth{ 1.0f };
	};

	struct _MPPAPI GraphSamplerBinding
	{
		std::string sampler;
		GraphImageHandle image;
	};

	struct _MPPAPI GraphPassInfo
	{
		std::string name;
		GraphPassType type{ GraphPassType::Scene };
		// XML stores this stable factory identifier, not executable code.
		std::string callbackFactory;
		// Resource name; resolution/validation occurs when a declarative
		// fullscreen pass implementation is attached.
		std::string programResource;
		std::vector<GraphImageHandle> sampledInputs;
		std::vector<GraphSamplerBinding> samplerBindings;
		UniformCollection parameters;
		std::vector<GraphColourOutput> colourOutputs;
		std::vector<GraphDepthOutput> depthOutputs;
	};

	struct _MPPAPI RenderGraphCompileResult	{
		bool valid{ false };
		std::vector<GraphPassHandle> passOrder;
		std::vector<std::string> diagnostics;
	};

	// One physical attachment requirement for a produced image version. The
	// first RG2 allocator may allocate one target per entry; later allocators
	// can alias entries with compatible, non-overlapping intervals.
	struct _MPPAPI GraphImageLifetime
	{
		GraphImageHandle image;
		GraphImageDesc desc;
		glm::uvec2 size{ 0 };
		uint32_t firstPass{ UINT32_MAX };
		uint32_t lastPass{ UINT32_MAX };
	};

	struct _MPPAPI RenderGraphAllocationPlan
	{
		bool valid{ false };
		std::vector<GraphImageLifetime> allocatedImages;
		std::vector<GraphImageHandle> importedImages;
		std::vector<std::string> diagnostics;
	};

	// Declarative topology only. Allocation/execution is deliberately separate
	// so validation is usable without an OpenGL context.
	class _MPPAPI RenderGraph
	{
		struct Image;
		struct Pass;

		std::vector<Image> mImages;
		std::vector<Pass> mPasses;

		bool validImage(GraphImageHandle image) const;
		bool validPass(GraphPassHandle pass) const;

	public:
		RenderGraph();
		~RenderGraph();
		RenderGraph(RenderGraph&&) noexcept;
		RenderGraph& operator =(RenderGraph&&) noexcept;
		RenderGraph(RenderGraph const&) = delete;
		RenderGraph& operator =(RenderGraph const&) = delete;

		GraphImageHandle createImage(std::string const& name, GraphImageDesc const& desc);
		void setImageImportName(GraphImageHandle image, std::string const& importName);
		GraphImageInfo getImageInfo(GraphImageHandle image) const;
		std::vector<GraphImageHandle> getImportedImages() const;
		GraphPassHandle addPass(std::string const& name, GraphPassType type = GraphPassType::Scene);
		void setPassProgramResource(GraphPassHandle pass, std::string const& resourceName);
		void setPassCallbackFactory(GraphPassHandle pass, std::string const& factoryName);

		void readSampled(GraphPassHandle pass, GraphImageHandle image);
		void bindSampler(GraphPassHandle pass, std::string const& sampler, GraphImageHandle image);
		void setPassParameters(GraphPassHandle pass, UniformCollection const& parameters);
		GraphImageHandle writeColour(GraphPassHandle pass, GraphImageHandle image, GraphLoadOp load = GraphLoadOp::DontCare, GraphStoreOp store = GraphStoreOp::Store, glm::vec4 const& clear = glm::vec4(0.0f));
		GraphImageHandle writeDepth(GraphPassHandle pass, GraphImageHandle image, GraphLoadOp load = GraphLoadOp::DontCare, GraphStoreOp store = GraphStoreOp::Store, float clear = 1.0f);

		size_t getImageCount() const;
		size_t getPassCount() const;
		GraphPassInfo getPassInfo(GraphPassHandle pass) const;

		RenderGraphCompileResult compile() const;
		RenderGraphCompileResult compile(Caps const& caps) const;
		RenderGraphAllocationPlan buildAllocationPlan(glm::uvec2 const& viewport) const;

		// Context-free diagnostic dump for logs and tests. It reports declared
		// images, their produced versions, and each pass's graph dependencies.
		std::string describe() const;
	};
}
