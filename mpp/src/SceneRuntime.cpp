#include <glew/glew.h>
#include <algorithm>
#include <filesystem>
#include <glm/gtc/constants.hpp>
#include "mpp/BoxModelStream.h"
#include "mpp/CylinderModelStream.h"
#include "mpp/GridModelStream.h"
#include "mpp/Model.h"
#include "mpp/MppException.h"
#include "mpp/MppModelStream.h"
#include "mpp/ProgrammaticPbrMaterialStream.h"
#include "mpp/ResourceManager.h"
#include "mpp/SceneRuntime.h"
#include "mpp/SphereModelStream.h"
#include "mpp/mesh/MeshSpecification.h"
#include "mpp/mesh/Vertex.h"

namespace mpp
{
	namespace
	{
		mesh::MeshSpecification previewMeshSpecification()
		{
			mesh::MeshSpecification result(mesh::Primitive::Type::Triangles);auto layout=result.createVertexBufferAttributeLayout(false);layout->createAttribute(mesh::Vertex::Component::Position3,mesh::Vertex::DataType::Float,false);layout->createAttribute(mesh::Vertex::Component::Normal3,mesh::Vertex::DataType::Float,false);layout->createAttribute(mesh::Vertex::Component::TexCoord2,mesh::Vertex::DataType::Float,false);layout->createAttribute(mesh::Vertex::Component::Colour4,mesh::Vertex::DataType::Float,true);layout->createAttribute(mesh::Vertex::Component::Tangent4,mesh::Vertex::DataType::Float,false);result.setStorageType(mesh::VertexBufferStorageType::Static);result.setIndexedVertices(true);return result;
		}
	}

	SceneRuntime::SceneRuntime(RenderSystem* renderSystem,ResourceManager* resourceManager):mRenderSystem(renderSystem),mResourceManager(resourceManager)
	{
		if(!mRenderSystem||!mResourceManager)THROW_MPP("SceneRuntime requires render and resource systems.",__LINE__,__FILE__,__func__);
	}
	SceneRuntime::~SceneRuntime(){clear();}

	void SceneRuntime::clearResources(ScenePtr& scene,std::vector<std::string>& names)
	{
		if(scene){scene->unload();scene.reset();}std::vector<std::string> all=names;for(auto const& root:names){auto children=mResourceManager->getResourceNamesWithPrefix(root+"/");all.insert(all.end(),children.begin(),children.end());}std::sort(all.begin(),all.end());all.erase(std::unique(all.begin(),all.end()),all.end());std::sort(all.begin(),all.end(),[](auto const& left,auto const& right){return left.size()<right.size();});for(auto const& name:all)if(!mResourceManager->isResourceAlias(name))if(auto resource=mResourceManager->getResource(name,true))resource->destroy();for(auto const& name:all)if(mResourceManager->getResource(name,true)){try{mResourceManager->deleteResource(name);}catch(...){}}names.clear();
	}
	void SceneRuntime::clear(){clearResources(mScene,mResourceNames);mDiagnostics.clear();mModelTriangles.clear();mUniqueTriangles=0;}

	bool SceneRuntime::rebuild(SceneDocument const& document,std::map<std::string,ResourcePtr> const& materialBindings)
	{
		mDiagnostics=document.validate();if(mDiagnostics.hasErrors())return false;ScenePtr candidate=std::make_shared<Scene>(mRenderSystem);std::vector<std::string> candidateNames;std::map<std::string,uint64_t> candidateTriangles;uint64_t candidateTriangleTotal=0;auto generation=std::to_string(++mGeneration),prefix="SceneRuntime."+generation+".";
		try
		{
			candidate->load();auto meshSpec=previewMeshSpecification();auto fallbackName=prefix+"FallbackPbr";auto materialStream=std::make_shared<ProgrammaticPbrMaterialStream>(mResourceManager);PbrMaterialSpecification::PbrSurface surface;surface.enabled=true;surface.metallicFactor=0.0f;surface.roughnessFactor=0.8f;materialStream->setMeshSpecification(meshSpec);materialStream->setSurface(surface);auto fallback=mResourceManager->declareResource(fallbackName,materialStream).first;fallback->load();if(mResourceManager->getResource(fallbackName+"/Program",true))candidateNames.push_back(fallbackName+"/Program");candidateNames.push_back(fallbackName);
			for(auto const& authored:document.models)
			{
				ResourcePtr material=fallback;auto mapped=materialBindings.find(authored.materialBinding);if(mapped!=materialBindings.end()&&mapped->second)material=mapped->second;else if(!authored.materialBinding.empty())mDiagnostics.warning("MPP-SCENE-RUNTIME-001","Material binding '"+authored.materialBinding+"' uses the neutral PBR placeholder.",{document.sourcePath},authored.id);
				auto makePrimitive=[&](std::string const& name,SceneModelSource source){ResourceStreamPtr stream;switch(source){case SceneModelSource::Box:stream=std::make_shared<BoxModelStream>(mResourceManager,meshSpec,material->getName(),authored.primitive.width,authored.primitive.height,authored.primitive.depth);break;case SceneModelSource::Sphere:stream=std::make_shared<SphereModelStream>(mResourceManager,meshSpec,material->getName(),authored.primitive.radius,(int)authored.primitive.resolution);break;case SceneModelSource::Cylinder:stream=std::make_shared<CylinderModelStream>(mResourceManager,meshSpec,material->getName(),authored.primitive.height,authored.primitive.radius,authored.primitive.topRadius,(int)authored.primitive.resolution);break;default:stream=std::make_shared<GridModelStream>(mResourceManager,meshSpec,material->getName(),authored.primitive.width,authored.primitive.depth,authored.primitive.segmentsX,authored.primitive.segmentsZ,authored.primitive.textureRepeatU,authored.primitive.textureRepeatV);break;}auto model=mResourceManager->declareResource(name,stream).first;candidateNames.push_back(name);model->load();return model;};
				ResourcePtr model;bool placeholder=false;auto modelName=prefix+authored.id;if(authored.source==SceneModelSource::MppModel){auto path=std::filesystem::path(authored.file);if(!path.is_absolute())path=std::filesystem::path(document.sourcePath).parent_path()/path;if(std::filesystem::exists(path)){try{auto stream=std::make_shared<MppModelStream>(mResourceManager,path.string());model=mResourceManager->declareResource(modelName+".Source",stream).first;candidateNames.push_back(modelName+".Source");model->load();}catch(std::exception const& error){mDiagnostics.warning("MPP-SCENE-RUNTIME-002","Model load failed; using placeholder: "+std::string(error.what()),{document.sourcePath},authored.id);model.reset();}}if(!model){mDiagnostics.warning("MPP-SCENE-RUNTIME-003","Missing model uses a placeholder box.",{document.sourcePath},authored.id);placeholder=true;model=makePrimitive(modelName+".Placeholder",SceneModelSource::Box);}}else model=makePrimitive(modelName,authored.source);
				auto triangles=placeholder?0u:(uint64_t)static_cast<Model*>(model.get())->getNumTriangles();candidateTriangles[authored.id]=triangles;if(authored.visible)candidateTriangleTotal+=triangles;auto instance=candidate->add3dModel(model);instance->resetTransform();instance->translate(authored.translation);instance->rotateSelf(glm::radians(authored.rotationDegrees.x),glm::vec3(1,0,0));instance->rotateSelf(glm::radians(authored.rotationDegrees.y),glm::vec3(0,1,0));instance->rotateSelf(glm::radians(authored.rotationDegrees.z),glm::vec3(0,0,1));instance->scale(authored.scale);uint32_t flags=0;if(authored.visible)flags|=ModelRenderParams::Flag_Visible;if(authored.shadowCaster)flags|=ModelRenderParams::Flag_CastShadows;instance->getParams()->setModelFlags(flags);if(authored.source!=SceneModelSource::MppModel)instance->getParams()->setModelMaterial(material);
			}
		}
		catch(std::exception const& error){mDiagnostics.error("MPP-SCENE-RUNTIME-004","Scene candidate creation failed: "+std::string(error.what()),{document.sourcePath},"scene");clearResources(candidate,candidateNames);return false;}
		auto previous=mScene;auto previousNames=std::move(mResourceNames);mScene=std::move(candidate);mResourceNames=std::move(candidateNames);mModelTriangles=std::move(candidateTriangles);mUniqueTriangles=candidateTriangleTotal;clearResources(previous,previousNames);return true;
	}
	ScenePtr const& SceneRuntime::getScene()const{return mScene;}
	DiagnosticBag const& SceneRuntime::getDiagnostics()const{return mDiagnostics;}
	uint64_t SceneRuntime::getUniqueTriangleCount()const{return mUniqueTriangles;}
	uint64_t SceneRuntime::getModelTriangleCount(std::string const& modelId)const{auto found=mModelTriangles.find(modelId);return found==mModelTriangles.end()?0:found->second;}
}
