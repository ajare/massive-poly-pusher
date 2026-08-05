#include <glew/glew.h>

#include <sstream>

#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphScenePass.h"
#include "mpp/RenderGraphTemplate.h"
#include "mpp/Caps.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/MppException.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderTexture.h"

using namespace std;

namespace mpp
{
	namespace
	{
		class GraphFramebufferTarget final : public RenderTarget
		{
			GLuint mFramebuffer{ 0 };
			vector<GLenum> mDrawBuffers;
			vector<RenderTexture*> mMipTargets;

			static RenderTexture* requireRenderTexture(RenderTargetPtr const& target)
			{
				auto texture = dynamic_cast<RenderTexture*>(target.get());
				if (!texture)
				{
					THROW_MPP("A multi-attachment render graph pass requires RenderTexture-backed outputs.", __LINE__, __FILE__, __func__);
				}
				return texture;
			}

			void deactivate() override
			{
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
				for (auto target : mMipTargets) target->generateMipMaps();
			}

			void activate() override
			{
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer));
				if (!mDrawBuffers.empty()) GL_CHECK(glDrawBuffers((GLsizei)mDrawBuffers.size(), mDrawBuffers.data()));
				else GL_CHECK(glDrawBuffer(GL_NONE));
			}

		public:
			GraphFramebufferTarget(string const& name, vector<RenderTargetPtr> const& colours, vector<uint32_t> const& colourMips, RenderTargetPtr const& depth, uint32_t depthMip)
				: RenderTarget(colours.empty() ? max<size_t>(1, depth->getWidth() >> depthMip) : max<size_t>(1, colours.front()->getWidth() >> colourMips.front()), colours.empty() ? max<size_t>(1, depth->getHeight() >> depthMip) : max<size_t>(1, colours.front()->getHeight() >> colourMips.front()))
			{
				GL_CHECK(glGenFramebuffers(1, &mFramebuffer));
				GL_CHECK(glObjectLabel(GL_FRAMEBUFFER, mFramebuffer, -1, ("RenderGraphPass: " + name).c_str()));
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer));
				for (size_t index = 0; index < colours.size(); ++index)
				{
					auto texture = requireRenderTexture(colours[index]);
					if (colourMips[index] == 0) mMipTargets.push_back(texture);
					if (max<size_t>(1, texture->getWidth() >> colourMips[index]) != mWidth || max<size_t>(1, texture->getHeight() >> colourMips[index]) != mHeight)
					{
						THROW_MPP("Render graph pass attachment dimensions do not match.", __LINE__, __FILE__, __func__);
					}
					GLenum attachment = (GLenum)(GL_COLOR_ATTACHMENT0 + index);
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture->getColourAttachmentId(0), (GLint)colourMips[index]));
					mDrawBuffers.push_back(attachment);
				}
				if (depth)
				{
					auto texture = requireRenderTexture(depth);
					if (depthMip == 0) mMipTargets.push_back(texture);
					if (max<size_t>(1, texture->getWidth() >> depthMip) != mWidth || max<size_t>(1, texture->getHeight() >> depthMip) != mHeight)
					{
						THROW_MPP("Render graph depth attachment dimensions do not match colour attachments.", __LINE__, __FILE__, __func__);
					}
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, texture->hasStencilBuffer() ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture->getDepthTextureId(), (GLint)depthMip));
				}
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				{
					THROW_MPP("Render graph pass framebuffer is incomplete.", __LINE__, __FILE__, __func__);
				}
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
			}

			~GraphFramebufferTarget() override
			{
				if (mFramebuffer != 0) glDeleteFramebuffers(1, &mFramebuffer);
			}
		};

		void clearPassOutputs(GraphPassInfo const& pass)
		{
			for (size_t index = 0; index < pass.colourOutputs.size(); ++index)
			{
				auto const& output = pass.colourOutputs[index];
				if (output.load == GraphLoadOp::Clear)
				{
					GL_CHECK(glClearBufferfv(GL_COLOR, (GLint)index, &output.clearColour.x));
				}
			}
			for (auto const& output : pass.depthOutputs)
			{
				if (output.load == GraphLoadOp::Clear)
				{
					GL_CHECK(glClearBufferfv(GL_DEPTH, 0, &output.clearDepth));
				}
			}
		}

		void discardDontCareOutputs(GraphPassInfo const& pass)
		{
			if (!GLEW_VERSION_4_3 && !GLEW_ARB_invalidate_subdata) return;
			vector<GLenum> attachments;
			for (size_t index = 0; index < pass.colourOutputs.size(); ++index)
			{
				if (pass.colourOutputs[index].store == GraphStoreOp::DontCare)
					attachments.push_back((GLenum)(GL_COLOR_ATTACHMENT0 + index));
			}
			for (auto const& output : pass.depthOutputs)
			{
				if (output.store == GraphStoreOp::DontCare) attachments.push_back(GL_DEPTH_ATTACHMENT);
			}
			if (!attachments.empty()) GL_CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, (GLsizei)attachments.size(), attachments.data()));
		}
	}

	RenderGraphExecutionContext::RenderGraphExecutionContext(RenderGraphTargets const* targets, UniformCollection const* parameters, RenderGraphFrameContext const* frame, GraphPassInfo const* pass)
		: mTargets(targets)
		, mParameters(parameters)
		, mFrame(frame)
		, mPass(pass)
	{
	}

	RenderTargetPtr RenderGraphExecutionContext::getImage(GraphImageHandle image) const
	{
		return mTargets ? mTargets->get(image) : nullptr;
	}

	UniformCollection const& RenderGraphExecutionContext::getParameters() const
	{
		if (!mParameters) THROW_MPP("Render graph pass has no parameter collection.", __LINE__, __FILE__, __func__);
		return *mParameters;
	}

	RenderGraphFrameContext const& RenderGraphExecutionContext::getFrame() const
	{
		if (!mFrame) THROW_MPP("Render graph execution has no frame context.", __LINE__, __FILE__, __func__);
		return *mFrame;
	}

	GraphPassInfo const& RenderGraphExecutionContext::getPass() const
	{
		if (!mPass) THROW_MPP("Render graph execution has no pass metadata.", __LINE__, __FILE__, __func__);
		return *mPass;
	}

	RenderGraphExecutor::RenderGraphExecutor(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
	{
		if (!mRenderSystem)
		{
			THROW_MPP("Render graph executor requires a RenderSystem.", __LINE__, __FILE__, __func__);
		}
	}

	RenderGraphExecutor::~RenderGraphExecutor() = default;

	void RenderGraphExecutor::setPassCallback(GraphPassHandle pass, function<void(RenderGraphExecutionContext const&)> callback)
	{
		if (!pass.isValid() || !callback)
		{
			THROW_MPP("Render graph pass callback requires a valid pass and function.", __LINE__, __FILE__, __func__);
		}
		mCallbacks[pass.id] = move(callback);
	}

	void RenderGraphExecutor::setPassFactoryRegistry(RenderGraphPassFactoryRegistry const* registry)
	{
		mFactoryRegistry = registry;
	}

	void RenderGraphExecutor::setFrameContext(RenderGraphFrameContext const* frameContext)
	{
		mFrameContext = frameContext;
	}

	void RenderGraphExecutor::setPassParameterOverrides(GraphPassHandle pass, UniformCollection const& parameters)
	{
		if (!pass.isValid()) THROW_MPP("Invalid render graph pass handle.", __LINE__, __FILE__, __func__);
		mParameterOverrides[pass.id] = parameters;
	}

	void RenderGraphExecutor::clearPassCallbacks()	{
		mCallbacks.clear();
		mScenePasses.clear();
	}

	void RenderGraphExecutor::execute(RenderGraph const& graph, RenderGraphTargets const& targets, Caps const& caps)
	{
		auto compiled = graph.compile(caps);
		if (!compiled.valid)
		{
			ostringstream message;
			message << "Cannot execute invalid render graph:";
			for (auto const& diagnostic : compiled.diagnostics) message << "\n- " << diagnostic;
			THROW_MPP(message.str(), __LINE__, __FILE__, __func__);
		}
		for (auto const passHandle : compiled.passOrder)
		{
			auto const pass = graph.getPassInfo(passHandle);
			auto const explicitCallback = mCallbacks.find(passHandle.id);
			RenderGraphPassCallback callback = explicitCallback == mCallbacks.end() ? RenderGraphPassCallback() : explicitCallback->second;
			RenderGraphScenePass* scenePass = nullptr;
			if (!callback && mFactoryRegistry && !pass.callbackFactory.empty())
			{
				callback = mFactoryRegistry->findFactory(pass.callbackFactory);
				if (!callback)
				{
					auto found = mScenePasses.find(passHandle.id);
					if (found == mScenePasses.end())
					{
						auto created = mFactoryRegistry->createScenePass(pass.callbackFactory);
						if (created) found = mScenePasses.emplace(passHandle.id, std::move(created)).first;
					}
					if (found != mScenePasses.end()) scenePass = found->second.get();
				}
			}
			auto override = mParameterOverrides.find(passHandle.id);
			RenderGraphExecutionContext context(&targets, override == mParameterOverrides.end() ? &pass.parameters : &override->second, mFrameContext, &pass);
			bool const declarativeFullscreen = !callback && !scenePass && mExecutingTemplate && pass.type == GraphPassType::Fullscreen && !pass.programResource.empty();
			if (!callback && !scenePass && !declarativeFullscreen)
			{
				THROW_MPP("No callback registered for render graph pass '" + pass.name + "'" +
					(pass.callbackFactory.empty() ? "." : " (factory '" + pass.callbackFactory + "')."), __LINE__, __FILE__, __func__);
			}
			vector<RenderTargetPtr> colours;
			vector<uint32_t> colourMips;
			for (auto const& output : pass.colourOutputs)
			{
				auto target = targets.get(output.image);
				if (!target) THROW_MPP("Render graph colour output has no allocated or imported target.", __LINE__, __FILE__, __func__);
				colours.push_back(target);
				colourMips.push_back(output.mipLevel);
			}
			RenderTargetPtr depth;
			uint32_t depthMip = 0;
			if (!pass.depthOutputs.empty())
			{
				depth = targets.get(pass.depthOutputs.front().image);
				depthMip = pass.depthOutputs.front().mipLevel;
				if (!depth) THROW_MPP("Render graph depth output has no allocated or imported target.", __LINE__, __FILE__, __func__);
			}

			if (colours.empty() && !depth)
			{
				THROW_MPP("Render graph executor supports graphics passes with at least one output.", __LINE__, __FILE__, __func__);
			}
			RenderTargetPtr passTarget;
			if (colours.size() == 1 && colourMips.front() == 0 && !depth)
			{
				passTarget = colours.front();
			}
			else
			{
				passTarget = make_shared<GraphFramebufferTarget>(pass.name, colours, colourMips, depth, depthMip);
			}
			map<RenderTexture*, uint32_t> mipViews;
			for (auto const& binding : pass.samplerBindings)
			{
				if (binding.mipLevel == UINT32_MAX) continue;
				auto texture = dynamic_cast<RenderTexture*>(targets.get(binding.image).get());
				if (!texture) THROW_MPP("Explicit graph mip views require RenderTexture inputs.", __LINE__, __FILE__, __func__);
				auto existing = mipViews.find(texture);
				if (existing != mipViews.end() && existing->second != binding.mipLevel)
					THROW_MPP("One graph pass cannot bind different mip views of the same texture without texture-view support.", __LINE__, __FILE__, __func__);
				mipViews[texture] = binding.mipLevel;
			}
			mRenderSystem->pushRenderTarget(passTarget);
			mRenderSystem->setExpectedGraphColourOutputs(pass.colourOutputs.size());
			bool const imagePass = pass.type == GraphPassType::Fullscreen || pass.type == GraphPassType::Present;
			GLboolean depthEnabled = GL_FALSE, cullEnabled = GL_FALSE, scissorEnabled = GL_FALSE, depthWriteEnabled = GL_TRUE;
			if (imagePass)
			{
				// Fullscreen programs use the renderer's identity 2D transform. Graph
				// execution enters from a 3D scene projection, so preserve it and
				// install the same transform used by the manual post-effect path.
				depthEnabled = glIsEnabled(GL_DEPTH_TEST);
				cullEnabled = glIsEnabled(GL_CULL_FACE);
				scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
				GL_CHECK(glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled));
				mRenderSystem->pushProjectionMatrix();
				mRenderSystem->pushCameraMatrix();
				mRenderSystem->pushModelMatrix();
				mRenderSystem->setProjection2dOrthographic();
				mRenderSystem->resetTransform();
				GL_CHECK(glDisable(GL_DEPTH_TEST));
				GL_CHECK(glDepthMask(GL_FALSE));
				GL_CHECK(glDisable(GL_CULL_FACE));
				GL_CHECK(glDisable(GL_SCISSOR_TEST));
			}
			auto restoreImagePassState = [&]()
			{
				if (!imagePass) return;
				if (depthEnabled) GL_CHECK(glEnable(GL_DEPTH_TEST)); else GL_CHECK(glDisable(GL_DEPTH_TEST));
				GL_CHECK(glDepthMask(depthWriteEnabled));
				if (cullEnabled) GL_CHECK(glEnable(GL_CULL_FACE)); else GL_CHECK(glDisable(GL_CULL_FACE));
				if (scissorEnabled) GL_CHECK(glEnable(GL_SCISSOR_TEST)); else GL_CHECK(glDisable(GL_SCISSOR_TEST));
				mRenderSystem->popModelMatrix();
				mRenderSystem->popCameraMatrix();
				mRenderSystem->popProjectionMatrix();
			};
			try
			{
				for (auto const& view : mipViews) view.first->applyMipView(view.second);
				mRenderSystem->setViewport(0, 0, passTarget->getWidth(), passTarget->getHeight());
				clearPassOutputs(pass);
				if (callback) callback(context);
				else if (scenePass) scenePass->execute(context);
				else
				{
					vector<pair<string, Texture*>> samplers;
					for (auto const& binding : pass.samplerBindings)
						samplers.push_back({ binding.sampler, dynamic_cast<Texture*>(context.getImage(binding.image).get()) });
					mRenderSystem->renderGraphFullscreen(mExecutingTemplate->getProgram(passHandle), samplers, context.getParameters());
				}
				discardDontCareOutputs(pass);
				for (auto const& view : mipViews) view.first->restoreMipView();
				restoreImagePassState();
				mRenderSystem->setExpectedGraphColourOutputs(0);
				mRenderSystem->popRenderTarget();
			}
			catch (...)
			{
				for (auto const& view : mipViews) view.first->restoreMipView();
				restoreImagePassState();
				mRenderSystem->setExpectedGraphColourOutputs(0);
				mRenderSystem->popRenderTarget();
				throw;
			}
		}
	}

	void RenderGraphExecutor::execute(RenderGraphTemplate const& graphTemplate, RenderGraphTargets const& targets, Caps const& caps)
	{
		if (!graphTemplate.getGraph()) THROW_MPP("Cannot execute an unloaded RenderGraphTemplate.", __LINE__, __FILE__, __func__);
		mExecutingTemplate = &graphTemplate;
		try
		{
			execute(*graphTemplate.getGraph(), targets, caps);
			mExecutingTemplate = nullptr;
		}
		catch (...)
		{
			mExecutingTemplate = nullptr;
			throw;
		}
	}
}
