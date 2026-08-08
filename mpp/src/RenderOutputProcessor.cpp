#include "mpp/Config.h"
#if MPP_PLATFORM == MPP_PLATFORM_WINDOWS
#include <Windows.h>
#endif
#include <glew/glew.h>

#include "mpp/RenderOutputProcessor.h"

#include <utility>

#include "mpp/MppException.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"

using namespace std;

namespace mpp
{
	namespace
	{
		GraphImageInfo findImage(RenderGraph const& graph, string const& name)
		{
			for(uint32_t id=0;id<graph.getImageCount();++id){auto info=graph.getImageInfo({id,0});if(info.name==name)return info;}
			THROW_MPP("Named output references unknown graph image '"+name+"'.",__LINE__,__FILE__,__func__);
		}

		bool samePlans(vector<RenderPipelineOutputPlan> const& left,vector<RenderPipelineOutputPlan> const& right)
		{
			if(left.size()!=right.size())return false;
			for(size_t index=0;index<left.size();++index)
			{
				auto const& a=left[index];auto const& b=right[index];
				if(a.name!=b.name||a.image!=b.image||a.taaDepth!=b.taaDepth||a.logicalSize!=b.logicalSize||a.rasterSamples!=b.rasterSamples||a.antiAliasing.msaa!=b.antiAliasing.msaa||a.antiAliasing.ssaa!=b.antiAliasing.ssaa||a.antiAliasing.taa!=b.antiAliasing.taa||a.antiAliasing.fxaa!=b.antiAliasing.fxaa||a.physicalImages.size()!=b.physicalImages.size())return false;
				for(size_t image=0;image<a.physicalImages.size();++image){auto const& x=a.physicalImages[image];auto const& y=b.physicalImages[image];if(x.role!=y.role||x.size!=y.size||x.format!=y.format||x.samples!=y.samples)return false;}
			}
			return true;
		}
	}

	RenderOutputProcessor::RenderOutputProcessor(RenderSystem* renderSystem,string pipelineName):mRenderSystem(renderSystem),mPipelineName(std::move(pipelineName))
	{
		if(!mRenderSystem)THROW_MPP("RenderOutputProcessor requires a RenderSystem.",__LINE__,__FILE__,__func__);
	}

	void RenderOutputProcessor::rebuild(vector<RenderPipelineOutput> const& outputs,RenderGraph const& graph,map<string,RenderTargetPtr> const& destinations,AntiAliasingDefaults const& defaults)
	{
		vector<RenderPipelineOutputPlan> plans;plans.reserve(outputs.size());
		for(auto const& output:outputs)
		{
			auto destination=destinations.find(output.name);if(destination==destinations.end()||!destination->second)THROW_MPP("Named output '"+output.name+"' has no destination render target.",__LINE__,__FILE__,__func__);
			auto colour=findImage(graph,output.image);auto effective=resolveAntiAliasing(defaults,output.antiAliasing);glm::uvec2 size((uint32_t)destination->second->getWidth(),(uint32_t)destination->second->getHeight());if(size.x==0||size.y==0)THROW_MPP("Named output '"+output.name+"' has zero-sized destination.",__LINE__,__FILE__,__func__);
			RenderPipelineOutputPlan plan;plan.name=output.name;plan.image=output.image;plan.taaDepth=output.taaDepth;plan.antiAliasing=effective;plan.logicalSize=size;plan.rasterSamples=antiAliasingSampleCount(effective.msaa);plan.physicalImages.push_back({PhysicalOutputImageRole::Input,size,colour.desc.format,1});
			if(effective.msaa!=AntiAliasingSamples::Off||effective.ssaa!=AntiAliasingSamples::Off||effective.taa||effective.fxaa){plan.physicalImages.push_back({PhysicalOutputImageRole::Work,size,colour.desc.format,1});plan.physicalImages.push_back({PhysicalOutputImageRole::Work,size,colour.desc.format,1});}
			if(effective.taa){plan.physicalImages.push_back({PhysicalOutputImageRole::TaaColourHistory,size,colour.desc.format,1});plan.physicalImages.push_back({PhysicalOutputImageRole::TaaColourHistory,size,colour.desc.format,1});auto depthFormat=output.taaDepth.empty()?GraphImageFormat::Depth24:findImage(graph,output.taaDepth).desc.format;plan.physicalImages.push_back({PhysicalOutputImageRole::TaaDepthHistory,size,depthFormat,1});}
			plans.push_back(std::move(plan));
		}
		if(samePlans(plans,mPlans))return;

		map<string,OutputState> candidate;uint64_t generation=mGeneration+1;
		for(auto const& plan:plans)
		{
			OutputState state;state.plan=plan;
			for(size_t index=0;index<plan.physicalImages.size();++index)
			{
				auto const& physical=plan.physicalImages[index];GraphImageDesc desc;desc.format=physical.format;desc.params.minFilter=GL_LINEAR;desc.params.magFilter=GL_LINEAR;desc.params.wrap=GL_CLAMP_TO_EDGE;auto target=mRenderSystem->createRenderTexture(mPipelineName+".Output."+plan.name+".g"+to_string(generation)+"."+to_string(index),physical.size.x,physical.size.y,makeGraphRenderTextureOptions(desc));
				switch(physical.role){case PhysicalOutputImageRole::Input:state.input=target;break;case PhysicalOutputImageRole::Work:state.work.push_back(target);break;case PhysicalOutputImageRole::TaaColourHistory:state.colourHistory.push_back(target);break;case PhysicalOutputImageRole::TaaDepthHistory:state.depthHistory=target;break;}
			}
			candidate.emplace(plan.name,std::move(state));
		}
		mOutputs.swap(candidate);mPlans.swap(plans);mGeneration=generation;
	}

	void RenderOutputProcessor::clear(){mOutputs.clear();mPlans.clear();++mGeneration;}
	RenderTargetPtr RenderOutputProcessor::getInput(string const& outputName) const{auto found=mOutputs.find(outputName);return found==mOutputs.end()?nullptr:found->second.input;}
	void RenderOutputProcessor::present(string const& outputName,RenderTargetPtr const& destination,RenderTargetPtr const& source) const
	{
		auto input=source?source:getInput(outputName);if(!input||!destination)THROW_MPP("Cannot present an unconfigured named output.",__LINE__,__FILE__,__func__);if(input==destination)return;auto texture=dynamic_cast<RenderTexture*>(input.get());if(!texture)THROW_MPP("Named output input is not a render texture.",__LINE__,__FILE__,__func__);mRenderSystem->setProjection2dOrthographic();mRenderSystem->resetTransform();mRenderSystem->scaleTransform2d(glm::vec2((float)destination->getWidth()/mRenderSystem->getWindowWidth(),(float)destination->getHeight()/mRenderSystem->getWindowHeight()));mRenderSystem->setRenderTarget(destination);mRenderSystem->setViewport(0,0,destination->getWidth(),destination->getHeight());mRenderSystem->renderFullscreenQuad(texture,BlendMode::One,BlendMode::Zero);
	}
	uint64_t RenderOutputProcessor::getGeneration() const{return mGeneration;}
	vector<RenderPipelineOutputPlan> const& RenderOutputProcessor::getPlans() const{return mPlans;}
}
