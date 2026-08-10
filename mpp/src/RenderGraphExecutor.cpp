#include <GL/glew.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <sstream>

#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphScenePass.h"
#include "mpp/RenderGraphTemplate.h"
#include "mpp/Caps.h"
#include "mpp/GLErrorCheck.h"
#include "mpp/GpuDebugScope.h"
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
				// glGenFramebuffers reserves a name; binding it creates the object.
				// Labeling a never-bound name is GL_INVALID_VALUE on strict drivers.
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer));
				GL_CHECK(glObjectLabel(GL_FRAMEBUFFER, mFramebuffer, -1, ("RenderGraphPass: " + name).c_str()));
				for (size_t index = 0; index < colours.size(); ++index)
				{
					auto texture = requireRenderTexture(colours[index]);
					if (colourMips[index] == 0) mMipTargets.push_back(texture);
					if (max<size_t>(1, texture->getWidth() >> colourMips[index]) != mWidth || max<size_t>(1, texture->getHeight() >> colourMips[index]) != mHeight)
					{
						THROW_MPP("Render graph pass attachment dimensions do not match.", __LINE__, __FILE__, __func__);
					}
					GLenum attachment = (GLenum)(GL_COLOR_ATTACHMENT0 + index);
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, texture->getAttachmentTextureTarget(), texture->getColourAttachmentId(0), (GLint)colourMips[index]));
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
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, texture->hasStencilBuffer() ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, texture->getAttachmentTextureTarget(), texture->getDepthTextureId(), (GLint)depthMip));
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
			GLboolean depthMask=GL_TRUE,colourMask[4]{GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE};GL_CHECK(glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask));GL_CHECK(glGetBooleanv(GL_COLOR_WRITEMASK,colourMask));GL_CHECK(glDepthMask(GL_TRUE));GL_CHECK(glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE));
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
			GL_CHECK(glDepthMask(depthMask));GL_CHECK(glColorMask(colourMask[0],colourMask[1],colourMask[2],colourMask[3]));
		}

		GLenum compareOp(GraphCompareOp value)
		{
			switch (value) { case GraphCompareOp::Never: return GL_NEVER; case GraphCompareOp::Less: return GL_LESS; case GraphCompareOp::Equal: return GL_EQUAL; case GraphCompareOp::LessEqual: return GL_LEQUAL; case GraphCompareOp::Greater: return GL_GREATER; case GraphCompareOp::NotEqual: return GL_NOTEQUAL; case GraphCompareOp::GreaterEqual: return GL_GEQUAL; default: return GL_ALWAYS; }
		}
		GLenum blendOp(GraphBlendOp value)
		{
			switch (value) { case GraphBlendOp::Add: return GL_FUNC_ADD; case GraphBlendOp::Subtract: return GL_FUNC_SUBTRACT; case GraphBlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT; case GraphBlendOp::Minimum: return GL_MIN; default: return GL_MAX; }
		}
		GLenum blendFactor(GraphBlendFactor value)
		{
			switch (value) { case GraphBlendFactor::Zero: return GL_ZERO; case GraphBlendFactor::One: return GL_ONE; case GraphBlendFactor::SourceColour: return GL_SRC_COLOR; case GraphBlendFactor::OneMinusSourceColour: return GL_ONE_MINUS_SRC_COLOR; case GraphBlendFactor::DestinationColour: return GL_DST_COLOR; case GraphBlendFactor::OneMinusDestinationColour: return GL_ONE_MINUS_DST_COLOR; case GraphBlendFactor::SourceAlpha: return GL_SRC_ALPHA; case GraphBlendFactor::OneMinusSourceAlpha: return GL_ONE_MINUS_SRC_ALPHA; case GraphBlendFactor::DestinationAlpha: return GL_DST_ALPHA; default: return GL_ONE_MINUS_DST_ALPHA; }
		}

		class GraphRasterStateScope
		{
			bool mActive{ false };
			GLboolean mDepth{ GL_FALSE }, mCull{ GL_FALSE }, mBlend{ GL_FALSE }, mMultisample{ GL_FALSE }, mAlphaToCoverage{ GL_FALSE }, mScissor{ GL_FALSE }, mDepthMask{ GL_TRUE };
			GLint mDepthFunc{ GL_LESS }, mCullMode{ GL_BACK }, mFrontFace{ GL_CCW }, mPolygonMode[2]{ GL_FILL, GL_FILL }, mBlendEquationRgb{ GL_FUNC_ADD }, mBlendEquationAlpha{ GL_FUNC_ADD };
			GLint mSourceRgb{ GL_ONE }, mDestinationRgb{ GL_ZERO }, mSourceAlpha{ GL_ONE }, mDestinationAlpha{ GL_ZERO }, mScissorBox[4]{};
			vector<array<GLboolean, 4>> mColourMasks;

			static void enabled(GLenum capability, bool value) { if (value) GL_CHECK(glEnable(capability)); else GL_CHECK(glDisable(capability)); }

		public:
			GraphRasterStateScope(GraphRasterState const& state, size_t colourOutputs, size_t width, size_t height)
			{
				if (!state.explicitState) return;
				mActive = true;
				mDepth = glIsEnabled(GL_DEPTH_TEST); mCull = glIsEnabled(GL_CULL_FACE); mBlend = glIsEnabled(GL_BLEND);
				mMultisample = glIsEnabled(GL_MULTISAMPLE); mAlphaToCoverage = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE); mScissor = glIsEnabled(GL_SCISSOR_TEST);
				GL_CHECK(glGetBooleanv(GL_DEPTH_WRITEMASK, &mDepthMask)); GL_CHECK(glGetIntegerv(GL_DEPTH_FUNC, &mDepthFunc));
				GL_CHECK(glGetIntegerv(GL_CULL_FACE_MODE, &mCullMode)); GL_CHECK(glGetIntegerv(GL_FRONT_FACE, &mFrontFace)); GL_CHECK(glGetIntegerv(GL_POLYGON_MODE, mPolygonMode));
				GL_CHECK(glGetIntegerv(GL_BLEND_EQUATION_RGB, &mBlendEquationRgb)); GL_CHECK(glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &mBlendEquationAlpha));
				GL_CHECK(glGetIntegerv(GL_BLEND_SRC_RGB, &mSourceRgb)); GL_CHECK(glGetIntegerv(GL_BLEND_DST_RGB, &mDestinationRgb)); GL_CHECK(glGetIntegerv(GL_BLEND_SRC_ALPHA, &mSourceAlpha)); GL_CHECK(glGetIntegerv(GL_BLEND_DST_ALPHA, &mDestinationAlpha));
				GL_CHECK(glGetIntegerv(GL_SCISSOR_BOX, mScissorBox));
				mColourMasks.resize(max<size_t>(1, colourOutputs));
				for (GLuint output = 0; output < mColourMasks.size(); ++output) GL_CHECK(glGetBooleani_v(GL_COLOR_WRITEMASK, output, mColourMasks[output].data()));

				enabled(GL_DEPTH_TEST, state.depthTest); GL_CHECK(glDepthMask(state.depthWrite ? GL_TRUE : GL_FALSE)); GL_CHECK(glDepthFunc(compareOp(state.depthCompare)));
				enabled(GL_CULL_FACE, state.cullMode != GraphCullMode::None); if (state.cullMode != GraphCullMode::None) GL_CHECK(glCullFace(state.cullMode == GraphCullMode::Front ? GL_FRONT : GL_BACK));
				GL_CHECK(glFrontFace(state.frontFace == GraphFrontFace::Clockwise ? GL_CW : GL_CCW)); GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, state.fillMode == GraphFillMode::Line ? GL_LINE : GL_FILL));
				enabled(GL_BLEND, state.blend); GL_CHECK(glBlendEquationSeparate(blendOp(state.colourBlendOp), blendOp(state.alphaBlendOp)));
				GL_CHECK(glBlendFuncSeparate(blendFactor(state.sourceColourBlend), blendFactor(state.destinationColourBlend), blendFactor(state.sourceAlphaBlend), blendFactor(state.destinationAlphaBlend)));
				enabled(GL_MULTISAMPLE, state.multisample); enabled(GL_SAMPLE_ALPHA_TO_COVERAGE, state.alphaToCoverage); enabled(GL_SCISSOR_TEST, state.scissor);
				if (state.scissor) GL_CHECK(glScissor((GLint)state.scissorRectangle.x, (GLint)state.scissorRectangle.y, (GLsizei)(state.scissorRectangle.z ? state.scissorRectangle.z : width), (GLsizei)(state.scissorRectangle.w ? state.scissorRectangle.w : height)));
				for (GLuint output = 0; output < mColourMasks.size(); ++output)
				{
					auto mask = output < state.colourWriteMasks.size() ? state.colourWriteMasks[output] : GraphColourWriteMask{};
					GL_CHECK(glColorMaski(output, mask.red, mask.green, mask.blue, mask.alpha));
				}
			}
			~GraphRasterStateScope()
			{
				if (!mActive) return;
				enabled(GL_DEPTH_TEST, mDepth != GL_FALSE); GL_CHECK(glDepthMask(mDepthMask)); GL_CHECK(glDepthFunc(mDepthFunc));
				enabled(GL_CULL_FACE, mCull != GL_FALSE); GL_CHECK(glCullFace(mCullMode)); GL_CHECK(glFrontFace(mFrontFace)); GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, mPolygonMode[0]));
				enabled(GL_BLEND, mBlend != GL_FALSE); GL_CHECK(glBlendEquationSeparate(mBlendEquationRgb, mBlendEquationAlpha)); GL_CHECK(glBlendFuncSeparate(mSourceRgb, mDestinationRgb, mSourceAlpha, mDestinationAlpha));
				enabled(GL_MULTISAMPLE, mMultisample != GL_FALSE); enabled(GL_SAMPLE_ALPHA_TO_COVERAGE, mAlphaToCoverage != GL_FALSE); enabled(GL_SCISSOR_TEST, mScissor != GL_FALSE); GL_CHECK(glScissor(mScissorBox[0], mScissorBox[1], mScissorBox[2], mScissorBox[3]));
				for (GLuint output = 0; output < mColourMasks.size(); ++output) GL_CHECK(glColorMaski(output, mColourMasks[output][0], mColourMasks[output][1], mColourMasks[output][2], mColourMasks[output][3]));
			}
		};

		void discardDontCareOutputs(GraphPassInfo const& pass, RenderGraphExecutionContext const& context)
		{
			if (!GLEW_VERSION_4_3 && !GLEW_ARB_invalidate_subdata) return;
			// The default framebuffer names its buffers GL_COLOR/GL_DEPTH/GL_STENCIL,
			// not GL_COLOR_ATTACHMENT0 -- passing an attachment enum there is
			// GL_INVALID_ENUM, which is what authoring store="dontCare" on a
			// presentation output used to produce.
			GLint boundFramebuffer = 0;
			GL_CHECK(glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &boundFramebuffer));
			bool const defaultFramebuffer = boundFramebuffer == 0;
			vector<GLenum> attachments;
			for (size_t index = 0; index < pass.colourOutputs.size(); ++index)
			{
				if (pass.colourOutputs[index].store != GraphStoreOp::DontCare) continue;
				if (!defaultFramebuffer) { attachments.push_back((GLenum)(GL_COLOR_ATTACHMENT0 + index)); continue; }
				// The default framebuffer has exactly one colour buffer however many
				// outputs the pass declares, so name it once.
				if (find(attachments.begin(), attachments.end(), (GLenum)GL_COLOR) == attachments.end()) attachments.push_back(GL_COLOR);
			}
			for (auto const& output : pass.depthOutputs)
			{
				if (output.store != GraphStoreOp::DontCare) continue;
				if (defaultFramebuffer) { attachments.push_back(GL_DEPTH); continue; }
				// GraphFramebufferTarget attaches a packed depth-stencil format as
				// GL_DEPTH_STENCIL_ATTACHMENT, so invalidating GL_DEPTH_ATTACHMENT
				// would name something that is not attached and leave the stencil
				// aspect stored regardless.
				auto const* texture = dynamic_cast<RenderTexture const*>(context.getImage(output.image).get());
				attachments.push_back(texture && texture->hasStencilBuffer() ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT);
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
		mGpuTimingSupported = GLEW_VERSION_3_3 || GLEW_ARB_timer_query;
	}

	RenderGraphExecutor::~RenderGraphExecutor()
	{
		clearGpuTimings();
	}

	void RenderGraphExecutor::clearGpuTimings()
	{
		for (auto const& frame : mPendingGpuTimings) for (auto const& query : frame)
		{
			GLuint ids[] = { query.begin, query.end };
			glDeleteQueries(2, ids);
		}
		mPendingGpuTimings.clear();
		mGpuTimings.clear();
	}

	void RenderGraphExecutor::collectGpuTimings()
	{
		while (!mPendingGpuTimings.empty())
		{
			auto const& frame = mPendingGpuTimings.front();
			bool available = true;
			for (auto const& query : frame)
			{
				GLint ready = GL_FALSE;
				GL_CHECK(glGetQueryObjectiv(query.end, GL_QUERY_RESULT_AVAILABLE, &ready));
				if (ready == GL_FALSE) { available = false; break; }
			}
			if (!available) break;
			for (auto const& query : frame)
			{
				GLuint64 begin = 0, end = 0;
				GL_CHECK(glGetQueryObjectui64v(query.begin, GL_QUERY_RESULT, &begin));
				GL_CHECK(glGetQueryObjectui64v(query.end, GL_QUERY_RESULT, &end));
				mGpuTimings[query.name] = { query.name, end >= begin ? double(end - begin) / 1000000.0 : 0.0 };
				GLuint ids[] = { query.begin, query.end };
				GL_CHECK(glDeleteQueries(2, ids));
			}
			mPendingGpuTimings.pop_front();
		}
	}


	void RenderGraphExecutor::setPassCallback(string const& passName, function<void(RenderGraphExecutionContext const&)> callback)
	{
		if (passName.empty() || !callback)
		{
			THROW_MPP("Render graph pass callback requires a pass name and function.", __LINE__, __FILE__, __func__);
		}
		mCallbacks[passName] = move(callback);
	}

	void RenderGraphExecutor::setPassCallback(RenderGraph const& graph, GraphPassHandle pass, function<void(RenderGraphExecutionContext const&)> callback)
	{
		setPassCallback(graph.getPassInfo(pass).name, move(callback));
	}

	void RenderGraphExecutor::setPassFactoryRegistry(RenderGraphPassFactoryRegistry const* registry)
	{
		mFactoryRegistry = registry;
	}

	void RenderGraphExecutor::setFrameContext(RenderGraphFrameContext const* frameContext)
	{
		mFrameContext = frameContext;
	}

	void RenderGraphExecutor::setPassParameterOverrides(string const& passName, UniformCollection const& parameters)
	{
		if (passName.empty()) THROW_MPP("Render graph pass parameter overrides require a pass name.", __LINE__, __FILE__, __func__);
		mParameterOverrides[passName] = parameters;
	}

	void RenderGraphExecutor::setPassParameterOverrides(RenderGraph const& graph, GraphPassHandle pass, UniformCollection const& parameters)
	{
		setPassParameterOverrides(graph.getPassInfo(pass).name, parameters);
	}

	void RenderGraphExecutor::clearPassCallbacks()	{
		mCallbacks.clear();
		mScenePasses.clear();
	}

	vector<GraphPassExecutionStats> const& RenderGraphExecutor::getLastExecutionStats() const
	{
		return mLastExecutionStats;
	}

	vector<GraphPassHandle> const& RenderGraphExecutor::getLastExecutionOrder() const
	{
		return mLastExecutionOrder;
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
		mLastExecutionStats.clear();
		if (mGpuTimingSupported) collectGpuTimings();
		vector<GpuTimingQuery> frameGpuQueries;
		auto cleanupQueries = [&](void*)
		{
			for (auto const& query : frameGpuQueries)
			{
				GLuint ids[] = { query.begin, query.end };
				glDeleteQueries(2, ids);
			}
		};
		unique_ptr<void, decltype(cleanupQueries)> queryCleanup((void*)1, cleanupQueries);
		bool const recordGpuTimings = mGpuTimingSupported && mPendingGpuTimings.size() < 8;
		for (auto const passHandle : compiled.passOrder)
		{
			auto const pass = graph.getPassInfo(passHandle);
			auto const statsBefore = mRenderSystem->getCurrentRenderInfo();
			auto const passStart = chrono::steady_clock::now();
			GpuDebugScope passScope(renderFlowPassRenderDocLabel(passHandle, pass.name, pass.type));
			auto const explicitCallback = mCallbacks.find(pass.name);
			RenderGraphPassCallback callback = explicitCallback == mCallbacks.end() ? RenderGraphPassCallback() : explicitCallback->second;
			RenderGraphScenePass* scenePass = nullptr;
			if (!callback && mFactoryRegistry && !pass.callbackFactory.empty())
			{
				callback = mFactoryRegistry->findFactory(pass.callbackFactory);
				if (!callback)
				{
					auto found = mScenePasses.find(pass.name);
					if (found == mScenePasses.end())
					{
						auto created = mFactoryRegistry->createScenePass(pass.callbackFactory);
						if (created) found = mScenePasses.emplace(pass.name, std::move(created)).first;
					}
					if (found != mScenePasses.end()) scenePass = found->second.get();
				}
			}
			auto override = mParameterOverrides.find(pass.name);
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
				auto target = targets.getWriteTarget(output.image);
				if (!target) THROW_MPP("Render graph colour output has no allocated or imported target.", __LINE__, __FILE__, __func__);
				colours.push_back(target);
				colourMips.push_back(output.mipLevel);
			}
			RenderTargetPtr depth;
			uint32_t depthMip = 0;
			if (!pass.depthOutputs.empty())
			{
				depth = targets.getWriteTarget(pass.depthOutputs.front().image);
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
				// The shared fullscreen quad is authored at window dimensions. Graph
				// targets commonly match an editor viewport instead, so scale the quad
				// to this pass target. Without this, every post-process samples only a
				// clipped UV subregion and ping-pong blur visibly drifts each pass.
				mRenderSystem->scaleTransform2d(glm::vec2(
					(float)passTarget->getWidth() / (float)mRenderSystem->getWindowWidth(),
					(float)passTarget->getHeight() / (float)mRenderSystem->getWindowHeight()));
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
			GpuTimingQuery gpuQuery;
			bool gpuQueryStarted = false;
			mRenderSystem->beginRenderFlowPass(passHandle, pass.name);
			try
			{
				if (recordGpuTimings)
				{
					gpuQuery.pass = passHandle;
					gpuQuery.name = pass.name;
					GLuint ids[2]{};
					GL_CHECK(glGenQueries(2, ids));
					gpuQuery.begin = ids[0]; gpuQuery.end = ids[1]; gpuQueryStarted = true;
					GL_CHECK(glQueryCounter(gpuQuery.begin, GL_TIMESTAMP));
				}
				for (auto const& view : mipViews) view.first->applyMipView(view.second);
				mRenderSystem->setViewport(0, 0, passTarget->getWidth(), passTarget->getHeight());
				{
					GpuDebugScope loadScope("Load/Clear Attachments");
					clearPassOutputs(pass);
				}
				{
					GpuDebugScope executeScope("Execute: " + pass.name);
					GraphRasterStateScope rasterState(pass.rasterState, pass.colourOutputs.size(), passTarget->getWidth(), passTarget->getHeight());
					if (callback) callback(context);
					else if (scenePass) scenePass->execute(context);
					else
					{
						vector<pair<string, Texture*>> samplers;
						for (auto const& binding : pass.samplerBindings)
							samplers.push_back({ binding.sampler, dynamic_cast<Texture*>(context.getImage(binding.image).get()) });
						mRenderSystem->renderGraphFullscreen(mExecutingTemplate->getProgram(passHandle), samplers, context.getParameters());
					}
				}
				{
					GpuDebugScope storeScope("Store/Resolve Attachments");
					discardDontCareOutputs(pass, context);
					for (auto const& output : pass.colourOutputs) if (output.store == GraphStoreOp::Store && targets.resolve(output.image, false) && mRenderSystem->isRenderFlowCaptureActive()){try{auto info=graph.getImageInfo(output.image);auto source=targets.getWriteTarget(output.image),destination=targets.get(output.image);RenderFlowResourceDesc sourceDesc{info.name+".v"+to_string(output.image.version)+".msaa",{(uint32_t)source->getWidth(),(uint32_t)source->getHeight()},info.desc.format,dynamic_cast<RenderTexture*>(source.get())->getSamples()};RenderFlowResourceDesc destinationDesc{info.name+".v"+to_string(output.image.version)+".resolved",{(uint32_t)destination->getWidth(),(uint32_t)destination->getHeight()},info.desc.format,1};mRenderSystem->recordRenderFlowEvent(RenderFlowEventKind::MsaaResolve,info.name+".v"+to_string(output.image.version),output.image,true,{}, {},false,{std::move(sourceDesc)},{std::move(destinationDesc)});}catch(...){mRenderSystem->failRenderFlowCapture();}}
					for (auto const& output : pass.depthOutputs) if (output.store == GraphStoreOp::Store && targets.resolve(output.image, true) && mRenderSystem->isRenderFlowCaptureActive()){try{auto info=graph.getImageInfo(output.image);auto source=targets.getWriteTarget(output.image),destination=targets.get(output.image);RenderFlowResourceDesc sourceDesc{info.name+".v"+to_string(output.image.version)+".msaa",{(uint32_t)source->getWidth(),(uint32_t)source->getHeight()},info.desc.format,dynamic_cast<RenderTexture*>(source.get())->getSamples()};RenderFlowResourceDesc destinationDesc{info.name+".v"+to_string(output.image.version)+".resolved",{(uint32_t)destination->getWidth(),(uint32_t)destination->getHeight()},info.desc.format,1};mRenderSystem->recordRenderFlowEvent(RenderFlowEventKind::MsaaResolve,info.name+".v"+to_string(output.image.version),output.image,true,{}, {},true,{std::move(sourceDesc)},{std::move(destinationDesc)});}catch(...){mRenderSystem->failRenderFlowCapture();}}
				}
				for (auto const& view : mipViews) view.first->restoreMipView();
				restoreImagePassState();
				mRenderSystem->setExpectedGraphColourOutputs(0);
				mRenderSystem->popRenderTarget();
				if (gpuQueryStarted)
				{
					GL_CHECK(glQueryCounter(gpuQuery.end, GL_TIMESTAMP));
					frameGpuQueries.push_back(gpuQuery);
					gpuQueryStarted = false;
				}
				auto const statsAfter = mRenderSystem->getCurrentRenderInfo();
				GraphPassExecutionStats stats;
				stats.pass = passHandle;
				stats.name = pass.name;
				stats.cpuMilliseconds = chrono::duration<double, milli>(chrono::steady_clock::now() - passStart).count();
				stats.gpuTimingSupported = mGpuTimingSupported;
				// Already name-keyed, so the defensive name comparison this carried
				// alongside an index key has become the lookup itself.
				auto gpuTiming = mGpuTimings.find(pass.name);
				if (gpuTiming != mGpuTimings.end()) { stats.gpuMilliseconds = gpuTiming->second.milliseconds; stats.gpuTimingAvailable = true; }
				stats.primitivesSubmitted = static_cast<uint64_t>(max(0, statsAfter.primitivesRendered - statsBefore.primitivesRendered));
				stats.trianglesSubmitted = static_cast<uint64_t>(max(0, statsAfter.trianglesRendered - statsBefore.trianglesRendered));
				stats.fullscreenQuads = static_cast<uint64_t>(max(0, statsAfter.fullscreenQuads - statsBefore.fullscreenQuads));
				mLastExecutionStats.push_back(move(stats));
				mRenderSystem->endRenderFlowPass(passHandle, pass.name);
			}
			catch (...)
			{
				if (gpuQueryStarted)
				{
					glQueryCounter(gpuQuery.end, GL_TIMESTAMP);
					GLuint ids[] = { gpuQuery.begin, gpuQuery.end };
					glDeleteQueries(2, ids);
					gpuQueryStarted = false;
				}
				for (auto const& view : mipViews) view.first->restoreMipView();
				restoreImagePassState();
				mRenderSystem->setExpectedGraphColourOutputs(0);
				mRenderSystem->popRenderTarget();
				mRenderSystem->abortRenderFlowPass();
				throw;
			}
		}
		if (!frameGpuQueries.empty()) mPendingGpuTimings.push_back(move(frameGpuQueries));
		mLastExecutionOrder = compiled.passOrder;
		queryCleanup.release();
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
