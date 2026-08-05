#include <glew/glew.h>

#include <sstream>

#include "mpp/RenderGraphExecutor.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
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
			}

			void activate() override
			{
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer));
				if (!mDrawBuffers.empty()) GL_CHECK(glDrawBuffers((GLsizei)mDrawBuffers.size(), mDrawBuffers.data()));
				else GL_CHECK(glDrawBuffer(GL_NONE));
			}

		public:
			GraphFramebufferTarget(string const& name, vector<RenderTargetPtr> const& colours, RenderTargetPtr const& depth)
				: RenderTarget(colours.empty() ? depth->getWidth() : colours.front()->getWidth(), colours.empty() ? depth->getHeight() : colours.front()->getHeight())
			{
				GL_CHECK(glGenFramebuffers(1, &mFramebuffer));
				GL_CHECK(glObjectLabel(GL_FRAMEBUFFER, mFramebuffer, -1, ("RenderGraphPass: " + name).c_str()));
				GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer));
				for (size_t index = 0; index < colours.size(); ++index)
				{
					auto texture = requireRenderTexture(colours[index]);
					if (texture->getWidth() != mWidth || texture->getHeight() != mHeight)
					{
						THROW_MPP("Render graph pass attachment dimensions do not match.", __LINE__, __FILE__, __func__);
					}
					GLenum attachment = (GLenum)(GL_COLOR_ATTACHMENT0 + index);
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture->getColourAttachmentId(0), 0));
					mDrawBuffers.push_back(attachment);
				}
				if (depth)
				{
					auto texture = requireRenderTexture(depth);
					if (texture->getWidth() != mWidth || texture->getHeight() != mHeight)
					{
						THROW_MPP("Render graph depth attachment dimensions do not match colour attachments.", __LINE__, __FILE__, __func__);
					}
					GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, texture->hasStencilBuffer() ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture->getDepthTextureId(), 0));
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

	RenderGraphExecutionContext::RenderGraphExecutionContext(RenderGraphTargets const* targets)
		: mTargets(targets)
	{
	}

	RenderTargetPtr RenderGraphExecutionContext::getImage(GraphImageHandle image) const
	{
		return mTargets ? mTargets->get(image) : nullptr;
	}

	RenderGraphExecutor::RenderGraphExecutor(RenderSystem* renderSystem)
		: mRenderSystem(renderSystem)
	{
		if (!mRenderSystem)
		{
			THROW_MPP("Render graph executor requires a RenderSystem.", __LINE__, __FILE__, __func__);
		}
	}

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

	void RenderGraphExecutor::clearPassCallbacks()
	{
		mCallbacks.clear();
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
		RenderGraphExecutionContext context(&targets);
		for (auto const passHandle : compiled.passOrder)
		{
			auto const pass = graph.getPassInfo(passHandle);
			auto const explicitCallback = mCallbacks.find(passHandle.id);
			RenderGraphPassCallback callback = explicitCallback == mCallbacks.end() ? RenderGraphPassCallback() : explicitCallback->second;
			if (!callback && mFactoryRegistry && !pass.callbackFactory.empty())
			{
				callback = mFactoryRegistry->findFactory(pass.callbackFactory);
			}
			if (!callback)
			{
				THROW_MPP("No callback registered for render graph pass '" + pass.name + "'" +
					(pass.callbackFactory.empty() ? "." : " (factory '" + pass.callbackFactory + "')."), __LINE__, __FILE__, __func__);
			}
			vector<RenderTargetPtr> colours;
			for (auto const& output : pass.colourOutputs)
			{
				auto target = targets.get(output.image);
				if (!target) THROW_MPP("Render graph colour output has no allocated or imported target.", __LINE__, __FILE__, __func__);
				colours.push_back(target);
			}
			RenderTargetPtr depth;
			if (!pass.depthOutputs.empty())
			{
				depth = targets.get(pass.depthOutputs.front().image);
				if (!depth) THROW_MPP("Render graph depth output has no allocated or imported target.", __LINE__, __FILE__, __func__);
			}

			if (colours.empty() && !depth)
			{
				THROW_MPP("Render graph executor supports graphics passes with at least one output.", __LINE__, __FILE__, __func__);
			}
			RenderTargetPtr passTarget;
			if (colours.size() == 1 && !depth)
			{
				passTarget = colours.front();
			}
			else
			{
				passTarget = make_shared<GraphFramebufferTarget>(pass.name, colours, depth);
			}
			mRenderSystem->pushRenderTarget(passTarget);
			try
			{
				mRenderSystem->setViewport(0, 0, passTarget->getWidth(), passTarget->getHeight());
				clearPassOutputs(pass);
				callback(context);
				discardDontCareOutputs(pass);
				mRenderSystem->popRenderTarget();
			}
			catch (...)
			{
				mRenderSystem->popRenderTarget();
				throw;
			}
		}
	}
}
