#pragma once

#include <memory>
#include <utility>

#include "mpp/Config.h"
#include "mpp/RenderGraph.h"

namespace mpp
{
	// Convenience authoring API over RenderGraph's versioned-handle model.
	// Runtime callbacks/import bindings remain executor concerns.
	class _MPPAPI RenderGraphBuilder
	{
		RenderGraph mGraph;

	public:
		class PassBuilder
		{
			RenderGraph* mGraph;
			GraphPassHandle mPass;

		public:
			PassBuilder(RenderGraph* graph, GraphPassHandle pass) : mGraph(graph), mPass(pass) {}
			GraphPassHandle getHandle() const { return mPass; }
			PassBuilder& sampled(GraphImageHandle image) { mGraph->readSampled(mPass, image); return *this; }
			PassBuilder& sampler(std::string const& name, GraphImageHandle image) { mGraph->bindSampler(mPass, name, image); return *this; }
			PassBuilder& program(std::string const& resource) { mGraph->setPassProgramResource(mPass, resource); return *this; }
			GraphImageHandle colour(GraphImageHandle image, GraphLoadOp load = GraphLoadOp::DontCare, GraphStoreOp store = GraphStoreOp::Store, glm::vec4 const& clear = glm::vec4(0.0f)) { return mGraph->writeColour(mPass, image, load, store, clear); }
			GraphImageHandle depth(GraphImageHandle image, GraphLoadOp load = GraphLoadOp::DontCare, GraphStoreOp store = GraphStoreOp::Store, float clear = 1.0f) { return mGraph->writeDepth(mPass, image, load, store, clear); }
			PassBuilder& callbackFactory(std::string const& name) { mGraph->setPassCallbackFactory(mPass, name); return *this; }
		};

		GraphImageHandle createImage(std::string const& name, GraphImageDesc const& desc) { return mGraph.createImage(name, desc); }
		GraphImageHandle importImage(std::string const& name, GraphImageDesc desc) { desc.external = true; desc.transient = false; return mGraph.createImage(name, desc); }
		PassBuilder addPass(std::string const& name) { return PassBuilder(&mGraph, mGraph.addPass(name)); }
		RenderGraph build() { return std::move(mGraph); }
	};
}
