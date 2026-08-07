#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "mpp/Config.h"
#include "mpp/Diagnostic.h"
#include "mpp/Resource.h"
#include "mpp/Scene.h"
#include "mpp/SceneDocument.h"

namespace mpp
{
	// Transactionally instantiates a SceneDocument. Failed candidates leave the
	// previous scene active; missing models become diagnosed placeholder boxes.
	class _MPPAPI SceneRuntime
	{
		RenderSystem* mRenderSystem;
		ResourceManager* mResourceManager;
		ScenePtr mScene;
		std::vector<std::string> mResourceNames;
		uint64_t mGeneration{0};
		DiagnosticBag mDiagnostics;
		void clearResources(ScenePtr& scene,std::vector<std::string>& names);
	public:
		SceneRuntime(RenderSystem* renderSystem,ResourceManager* resourceManager);
		~SceneRuntime();
		bool rebuild(SceneDocument const& document,std::map<std::string,ResourcePtr> const& materialBindings={});
		void clear();
		ScenePtr const& getScene()const;
		DiagnosticBag const& getDiagnostics()const;
	};
}
