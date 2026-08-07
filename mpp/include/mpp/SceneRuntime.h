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
#include "mpp/PbrLight.h"

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
		std::map<std::string,uint64_t> mModelTriangles;
		std::map<std::string,SceneModel3dPtr> mModelInstances;
		std::vector<PbrLight> mLights;
		std::string mEnvironmentBinding;
		uint64_t mUniqueTriangles{0};
		void clearResources(ScenePtr& scene,std::vector<std::string>& names);
	public:
		SceneRuntime(RenderSystem* renderSystem,ResourceManager* resourceManager);
		~SceneRuntime();
		bool rebuild(SceneDocument const& document,std::map<std::string,ResourcePtr> const& materialBindings={},std::map<std::string,UniformCollection> const& instanceOverrides={},std::string const& expectedEnvironmentBinding={});
		void clear();
		ScenePtr const& getScene()const;
		DiagnosticBag const& getDiagnostics()const;
		uint64_t getUniqueTriangleCount()const;
		uint64_t getModelTriangleCount(std::string const& modelId)const;
		SceneModel3dPtr getModelInstance(std::string const& modelId)const;
		std::vector<PbrLight> const& getLights()const;
		std::string const& getEnvironmentBinding()const;
	};
}
