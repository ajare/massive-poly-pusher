#include <algorithm>
#include <stdexcept>
#include "mpp/PbrMaterial.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"
#include "mpp/ResourceManager.h"
#include "mpp/Texture.h"
#include "mpp/resource-parsers/FilePbrPipelineStream.h"
#include "mpp/resource-parsers/PbrPipelineRuntime.h"
#include "mpp/resource-parsers/PbrPipelineResourceValidator.h"

namespace mpp::resource_parsers
{
	PbrPipelineRuntime::PbrPipelineRuntime(RenderSystem* renderSystem,ResourceManager* resourceManager):mRenderSystem(renderSystem),mResourceManager(resourceManager)
	{
		if(!mRenderSystem||!mResourceManager)throw std::invalid_argument("PbrPipelineRuntime requires render and resource systems.");
	}
	PbrPipelineRuntime::~PbrPipelineRuntime(){clear();}

	ResourcePtr PbrPipelineRuntime::resolve(std::string const& reference,std::string const& root)const
	{
		if(reference.empty())return {};if(auto owned=mResourceManager->getResource(root+"/"+reference,true))return owned;return mResourceManager->getResource(reference,true);
	}

	bool PbrPipelineRuntime::rebuild(std::shared_ptr<PbrPipelineDocument> document,uint32_t viewportWidth,uint32_t viewportHeight)
	{
		DiagnosticBag candidateDiagnostics;if(!document){candidateDiagnostics.error("MPP-PIPELINE-RUNTIME-001","Pipeline document is missing.");mDiagnostics=candidateDiagnostics;return false;}candidateDiagnostics=document->validate(mRenderSystem->getCaps());candidateDiagnostics.append(validatePbrPipelineResourceDefinitions(*document));if(candidateDiagnostics.hasErrors()){mDiagnostics=candidateDiagnostics;return false;}auto suffix=std::to_string(++mGeneration),candidateRoot="PbrPipelineRuntime."+suffix;std::map<std::string,ResourcePtr> candidateBindings;std::map<std::string,UniformCollection> candidateOverrides;std::map<std::string,RenderTargetPtr> candidateImports;RenderTargetPtr candidatePresentation;PbrEnvironmentPtr candidateEnvironment=std::make_shared<PbrEnvironment>();bool rootDeclared=false;
		try
		{
			auto stream=std::make_shared<FilePbrPipelineStream>(mResourceManager,document,document->sourcePath);auto resource=mResourceManager->declareResource(candidateRoot,stream).first;rootDeclared=true;resource->load();resource->create();
			for(auto const& binding:document->previewBindings){auto material=resolve(binding.materialResource,candidateRoot);if(!material){candidateDiagnostics.warning("MPP-PIPELINE-RUNTIME-002","Preview material '"+binding.materialResource+"' is unavailable; the conspicuous neutral material will be used.",{document->sourcePath},binding.binding);continue;}if(!dynamic_cast<PbrMaterial*>(material.get())){candidateDiagnostics.error("MPP-PIPELINE-RUNTIME-003","Preview binding '"+binding.binding+"' does not resolve to PbrMaterial.",{document->sourcePath},binding.binding);continue;}material->load();candidateBindings[binding.binding]=material;}
			for(auto const& overrideValue:document->previewOverrides){auto material=candidateBindings.find(overrideValue.binding);if(material==candidateBindings.end()){candidateDiagnostics.warning("MPP-PIPELINE-RUNTIME-004","Preview override for model '"+overrideValue.modelId+"' is inactive because its material binding is unresolved.",{document->sourcePath},overrideValue.modelId);continue;}static_cast<PbrMaterial*>(material->second.get())->validateInstanceUniforms(overrideValue.values);candidateOverrides[overrideValue.modelId]=overrideValue.values;}
			auto texture=[&](std::string const& reference,std::string const& fallback,char const* component){auto value=resolve(reference,candidateRoot);if(value&&!dynamic_cast<Texture*>(value.get())){candidateDiagnostics.error("MPP-PIPELINE-RUNTIME-005",std::string("Environment ")+component+" is not a Texture.",{document->sourcePath},document->environment.binding);return ResourcePtr();}if(!value){value=mResourceManager->getResource(fallback,true);candidateDiagnostics.warning("MPP-PIPELINE-RUNTIME-006",std::string("Environment ")+component+" uses the documented neutral fallback.",{document->sourcePath},document->environment.binding);}if(value)value->load();return value;};
			candidateEnvironment->irradianceMap=texture(document->environment.irradiance,"__mpp_tex_pbr_ibl_cube__","irradiance");candidateEnvironment->prefilteredSpecularMap=texture(document->environment.prefilteredSpecular,"__mpp_tex_pbr_ibl_cube__","prefiltered specular");candidateEnvironment->brdfIntegrationLut=texture(document->environment.brdfLut,"__mpp_tex_pbr_brdf_lut__","BRDF LUT");candidateEnvironment->backgroundMap=texture(document->environment.background,"__mpp_tex_pbr_ibl_cube__","background");
			for(auto const& import:document->imports){GraphImageDesc desc;desc.format=import.format;desc.usage=import.usage;desc.external=true;desc.transient=false;for(auto image:document->graph->getImportedImages()){auto info=document->graph->getImageInfo(image);if(info.importName==import.id||info.importName==import.semantic){desc=info.desc;break;}}auto width=desc.absoluteSize.x?desc.absoluteSize.x:std::max(1u,(uint32_t)(viewportWidth*desc.relativeSize.x));auto height=desc.absoluteSize.y?desc.absoluteSize.y:std::max(1u,(uint32_t)(viewportHeight*desc.relativeSize.y));auto target=mRenderSystem->createRenderTexture("PbrPipelineRuntime.Import."+import.id+"."+suffix,width,height,makeGraphRenderTextureOptions(desc));candidateImports[import.id]=target;candidateImports[import.semantic]=target;if(import.id=="screen"||import.semantic=="presentation")candidatePresentation=target;}
			if(candidateDiagnostics.hasErrors())throw std::runtime_error("Pipeline runtime resource validation failed.");
		}
		catch(std::exception const& error){candidateDiagnostics.error("MPP-PIPELINE-RUNTIME-007",std::string("Pipeline candidate creation failed: ")+error.what(),{document->sourcePath},"pipeline");if(rootDeclared){try{mResourceManager->deleteResourceTree(candidateRoot);}catch(...){}}mDiagnostics=candidateDiagnostics;return false;}
		accept();mPreviousRootResource=std::move(mRootResource);mPreviousDocument=std::move(mDocument);mPreviousMaterialBindings=std::move(mMaterialBindings);mPreviousInstanceOverrides=std::move(mInstanceOverrides);mPreviousImports=std::move(mImports);mPreviousPresentationTarget=std::move(mPresentationTarget);mPreviousEnvironment=std::move(mEnvironment);mRootResource=candidateRoot;mDocument=std::move(document);mMaterialBindings=std::move(candidateBindings);mInstanceOverrides=std::move(candidateOverrides);mImports=std::move(candidateImports);mPresentationTarget=std::move(candidatePresentation);mEnvironment=std::move(candidateEnvironment);mDiagnostics=std::move(candidateDiagnostics);return true;
	}

	void PbrPipelineRuntime::accept(){if(!mPreviousRootResource.empty()&&mResourceManager->getResource(mPreviousRootResource,true))mResourceManager->deleteResourceTree(mPreviousRootResource);mPreviousRootResource.clear();mPreviousDocument.reset();mPreviousMaterialBindings.clear();mPreviousInstanceOverrides.clear();mPreviousImports.clear();mPreviousPresentationTarget.reset();mPreviousEnvironment.reset();}
	void PbrPipelineRuntime::rollback(){if(!mPreviousDocument){if(!mRootResource.empty()&&mResourceManager->getResource(mRootResource,true))mResourceManager->deleteResourceTree(mRootResource);mRootResource.clear();mDocument.reset();mMaterialBindings.clear();mInstanceOverrides.clear();mImports.clear();mPresentationTarget.reset();mEnvironment.reset();return;}if(!mRootResource.empty()&&mResourceManager->getResource(mRootResource,true))mResourceManager->deleteResourceTree(mRootResource);mRootResource=std::move(mPreviousRootResource);mDocument=std::move(mPreviousDocument);mMaterialBindings=std::move(mPreviousMaterialBindings);mInstanceOverrides=std::move(mPreviousInstanceOverrides);mImports=std::move(mPreviousImports);mPresentationTarget=std::move(mPreviousPresentationTarget);mEnvironment=std::move(mPreviousEnvironment);}
	void PbrPipelineRuntime::clear(){if(!mRootResource.empty()&&mResourceManager->getResource(mRootResource,true))mResourceManager->deleteResourceTree(mRootResource);mRootResource.clear();mDocument.reset();mMaterialBindings.clear();mInstanceOverrides.clear();mImports.clear();mPresentationTarget.reset();mEnvironment.reset();accept();mDiagnostics.clear();}
	void PbrPipelineRuntime::resize(uint32_t width,uint32_t height){if(mPresentationTarget)mPresentationTarget->resize(width,height);}
	std::shared_ptr<PbrPipelineDocument> const& PbrPipelineRuntime::getDocument()const{return mDocument;}
	std::map<std::string,ResourcePtr> const& PbrPipelineRuntime::getMaterialBindings()const{return mMaterialBindings;}
	std::map<std::string,UniformCollection> const& PbrPipelineRuntime::getInstanceOverrides()const{return mInstanceOverrides;}
	std::map<std::string,RenderTargetPtr> const& PbrPipelineRuntime::getImports()const{return mImports;}
	RenderTargetPtr const& PbrPipelineRuntime::getPresentationTarget()const{return mPresentationTarget;}
	PbrEnvironmentPtr const& PbrPipelineRuntime::getEnvironment()const{return mEnvironment;}
	ResourcePtr PbrPipelineRuntime::getResolvedResource(std::string const& reference)const{return resolve(reference,mRootResource);}
	DiagnosticBag const& PbrPipelineRuntime::getDiagnostics()const{return mDiagnostics;}
}
