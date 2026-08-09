#pragma once

#include <cstdlib>
#include <map>
#include <deque>
#include <queue>
#include <stack>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Caps.h"
#include "mpp/AntiAliasing.h"
#include "mpp/ResourceWrangler.h"
#include "mpp/Pool.h"
#include "mpp/Resource.h"
#include "mpp/RenderPipeline.h"
#include "mpp/RenderTarget.h"
#include "mpp/RenderTexture.h"
#include "mpp/ClipRectangle.h"
#include "mpp/BlendMode.h"
#include "mpp/RenderInfo.h"
#include "mpp/Colour.h"
#include "mpp/Model.h"
#include "mpp/Font.h"
#include "mpp/Texture.h"
#include "mpp/TextureTile.h"
#include "mpp/QuadBatch.h"
#include "mpp/LineBatch.h"
#include "mpp/ModelRenderParams.h"
#include "mpp/ModelInstance.h"
#include "mpp/UniformCollection.h"
#include "mpp/UniformBuffer.h"
#include "mpp/Vertex2d.h"
#include "mpp/Logger.h"
#include "mpp/PbrLight.h"
#include "mpp/SceneFactory.h"

namespace mpp
{
	class Program;
	class Profiler; // Forward-declared so as to not pollute client apps.
	class ResourceManager;

	class _MPPAPI RenderSystem : public ResourceWrangler
	{
		enum class ProjectionType
		{
			Unknown,
			Ortho2D,
			Perspective3D
		};

	public:

		enum class TimeUnit
		{
			Seconds,
			Milliseconds,
			Microseconds,
			Nanoseconds
		};

		enum class SizeUnit
		{
			Bytes,
			Kilobytes,
			Megabytes,
			Gigabytes
		};

	public:

		enum class TextureTiling
		{
			Stretch,
			Tile
		};

		enum class TextureDiagnosticMode
		{
			Colour,
			Red,
			Green,
			Blue,
			Alpha,
			Luminance,
			Depth,
			HdrToneMap,
			HdrHeatMap
		};

		struct TextureDiagnosticOptions
		{
			TextureDiagnosticMode mode{ TextureDiagnosticMode::Colour };
			uint32_t mipLevel{ 0 };
			float exposure{ 1.0f };
			float depthNear{ 0.1f };
			float depthFar{ 100.0f };
		};

	private:

		struct SortedRenderCommand
		{
			uint64_t key;
			VertexBufferRenderCommand cmd;
			MeshInstance* meshInstance;
		};

	private:

		typedef std::pair<uint64_t, MeshInstance*> SortableMeshInstance;

		const uint32_t MaxTextGlyphs = 4096;
		
	private:

		Logger* mLogger;

		RenderSystemOptions mOptions;

		size_t mWindowWidth, mWindowHeight;

		size_t mViewportWidth, mViewportHeight;

		ResourceManager* mResourceMgr;

		Caps mCaps;

		//
		// Rendering
		//
		std::map<std::string, RenderPipelinePtr> mPipelines;

		RenderTargetPtr mRenderTarget;

		std::stack<RenderTargetPtr> mRenderTargetStack;

		std::stack<ClipRectangle> mClipStack;

		RenderTargetPtr mScreen;
		uint64_t mFrameSerial{ 0 };

		// Set only while a PBR pipeline is flushing its scene pass. Environment
		// samplers then override per-material placeholder bindings.
		PbrEnvironmentPtr mActivePbrEnvironment;

		// Pipeline-owned samplers (environment now; shadow maps later) override
		// material bindings by shader sampler name during a scene flush.
		std::map<std::string, ResourcePtr> mActivePipelineSamplerOverrides;

		struct ShadowDomainState
		{
			ShadowOptions options;
			RenderTargetPtr depthTarget;
			std::shared_ptr<UniformBuffer> frameBuffer;
		};
		std::map<std::string, ShadowDomainState> mShadowDomains;
		RenderTargetPtr mActiveShadowDepthTarget;
		std::shared_ptr<UniformBuffer> mShadowDisabledFrameBuffer;

		ProjectionType mProjectionType;

		// List of models to render
		Pool<ModelInstance>* mModelInstances;

		Pool<MeshInstance>* mMeshInstances;

		float mGamma;

		//
		// Core resources
		//

		// Default material: 2d with PTC and no texture
		ResourcePtr mDefaultMaterial, mInternalMaterial;

		// Default programs
		ResourcePtr mDefaultProgram2d, mDefaultProgram3d;

		ResourcePtr mActiveProgram;
		size_t mExpectedGraphColourOutputs{ 0 };

		// Render-thread-only, non-owning process-flow recorder state. A pipeline
		// owns the mutable candidate until endRenderFlowCapture() succeeds.
		RenderPipelineFlowSnapshot* mFlowCapture{ nullptr };
		GraphPassHandle mCurrentFlowPass;
		uint64_t mFlowSequence{ 0 };
		bool mFlowCaptureFailed{ false };
		size_t mFlowBatchHighWater{ 0 };
		size_t mFlowEventHighWater{ 0 };

		// Internal programs
		ResourcePtr mInternalProgram2d;
		ResourcePtr mShadowDepthProgram;

		// Internal textures
		ResourcePtr mNoTexture;

		ResourcePtr mInternalFontTexture;

		// Fullscreen effects
		ResourcePtr mFullscreenQuad, mFullscreenProgram, mToneMapProgram, mTextureDiagnosticProgram;
		ResourcePtr mBloomExtractProgram, mBloomBlurProgram, mBloomCombineProgram, mSsaaLanczosProgram, mTaaProgram, mFxaaProgram;

		// Text rendering
		ResourcePtr mTextMesh, mColouredTextMesh;
		bool mTextAsPoints{ false };

		// Lookup for ease
		std::vector<ResourcePtr> mCoreResources;

		//
		// Internal font
		//
		Font* mInternalFont;

		std::shared_ptr<UniformCollection> mTextUniforms;

		std::shared_ptr<ModelRenderParams> mTextParams;

		//
		// Internal buffer objects for 2d rendering
		//
		uint32_t mInternalVBO, mInternalIBO;

		//
		// 3d Transforms
		//
#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::mat4 m3dModelCameraProjectionMatrix;
		alignas(16) glm::mat4 m3dCameraMatrix;
		alignas(16) glm::mat4 m3dProjectionMatrix;
		alignas(16) glm::mat4 m3dModelMatrix;

		alignas(16) std::stack<glm::mat4> m3dCameraMatrixStack;
		alignas(16) std::stack<glm::mat4> m3dProjectionMatrixStack;
		alignas(16) std::stack<glm::mat4> m3dModelMatrixStack;
#pragma warning(pop)

		float mFarPlaneDistance;

#ifdef MPP_PROFILE_BUILD
		Profiler* mProfiler;

		LineBatch* mProfileLines;

		int mSamplesToRecord;

		struct TimeSample
		{
			int frameTime,
				driverWaitsGPU,
				driverWaitsKernel,
				driverWaitsLock,
				driverWaitsRender,
				driverWaitsSwap;
		};

		std::deque<TimeSample> mProfileTimeSamples;
#endif
		
		//
		// Debug and info
		//
		std::vector<std::string> mDebugPreMessages;
		
		std::vector<std::string> mDebugPostMessages;

		bool mShowDebugPanel;

		TimeUnit mTimeUnit;

		SizeUnit mSizeUnit;
		
		RenderInfo mRenderInfo;

		//
		// Built-in lights
		//
		UniformBuffer* mLightsBuffer{ nullptr };
		UniformBuffer* mPbrLightsBuffer{ nullptr };
		static constexpr size_t MaxPbrLights{ 8 };

		//
		// Scenes
		//
		std::map<std::string, SceneFactory> mSceneFactories;

	private:

		void checkExtensions();

		void checkCaps();

		void renderDebugPanel();
		
		// Methods to be used by Resources
		friend class Program;
		friend class Texture;
		friend class Material;
		friend class Model;
		friend class RenderGraphTargets;
		friend class RenderOutputProcessor;

		void useDefaultProgram();

		void setUsedProgram(ResourcePtr program);

		ResourcePtr getUsedProgram();
		
		void setDefaultTexture();

		int buildTextVertexBuffer(VertexBuffer* buffer, std::string const& text, int& offset, int x, int y);

		int buildColouredTextVertexBuffer(VertexBuffer* buffer, std::string const& text, int& offset, int x, int y);

		void createLightsData();

		void destroyLightsData();

		void createPbrLightsData();

		void destroyPbrLightsData();

		void destroyShadowDomains();

		void createShadowDomainResources(std::string const& name, ShadowDomainState& domain);

		void createShadowDisabledFrameBuffer();

		void addCoreResource(ResourcePtr resource, bool load);

		void setupRenderMeshInstance(MeshInstance* meshInstance, VertexBufferRenderCommand const& renderCmd, uint64_t sortKey, uint64_t* currentProgramKey, std::vector<uint64_t>* currentTextureKeys, Material** currentMaterial, std::vector<std::string>* flowStateChanges);

		void teardownRenderMeshInstance(MeshInstance* meshInstance);

		RenderTargetPtr createPhysicalRenderTexture(std::string const& name, size_t width, size_t height, RenderTextureOptions const& options, uint32_t samples);
		void renderSsaaLanczos(RenderTexture* source, RenderTargetPtr const& destination, glm::vec2 const& direction);
		void renderTaa(RenderTexture* currentColour, RenderTexture* currentDepth, RenderTexture* historyColour, RenderTexture* historyDepth, RenderTargetPtr const& destination, glm::mat4 const& inverseCurrentViewProjection, glm::mat4 const& previousViewProjection);
		void renderFxaa(RenderTexture* source, RenderTargetPtr const& destination);

	public:

		class _MPPAPI CubemapFaceRenderScope
		{
			RenderSystem* mSystem{}; RenderTexture* mTarget{};
			int mViewport[4]{}; int mScissor[4]{}; int mDrawBuffer{}; int mReadBuffer{}; bool mScissorEnabled{};
			bool mFinished{};
		public:
			CubemapFaceRenderScope(RenderSystem& system, RenderTargetPtr const& target, uint32_t face, uint32_t mipLevel);
			CubemapFaceRenderScope(CubemapFaceRenderScope const&) = delete;
			CubemapFaceRenderScope& operator=(CubemapFaceRenderScope const&) = delete;
			~CubemapFaceRenderScope();
			void finish();
		};

		RenderSystem(size_t windowWidth, size_t windowHeight, Logger* logger, RenderSystemOptions options = {});

		virtual ~RenderSystem();

		size_t getWindowWidth() const;

		size_t getWindowHeight() const;

		float getAspectRatio() const;

		Caps const& getCaps() const;
		RenderSystemOptions const& getOptions() const { return mOptions; }
		ResourceManager* getResourceManager() const { return mResourceMgr; }

		void initialise();

		void createCoreResources(ResourceManager* resourceMgr);

		void destroyCoreResources();

		void showDebugPanel(bool show, TimeUnit timeUnit = TimeUnit::Milliseconds, SizeUnit sizeUnit = SizeUnit::Megabytes);

		bool isDebugPanelShown() const;

		void setDebugPreMessages(std::vector<std::string> const& messages);

		void setDebugPostMessages(std::vector<std::string> const& messages);

		void setDefaultState();

		void setDisplay(int width, int height);

		void debugMessage(std::string const& message);

		void infoMessage(std::string const& message);

		void warnMessage(std::string const& message);

		void errorMessage(std::string const& message);

		// Render targets and textures
		void setRenderTarget(RenderTargetPtr renderTarget);

		void pushRenderTarget(RenderTargetPtr renderTarget);

		void popRenderTarget();

		void renderToScreen();

		// External/presentation target used by explicit render-graph pipelines.
		RenderTargetPtr getScreenRenderTarget() const;

		RenderTargetPtr createRenderTexture(std::string const& name, size_t width, size_t height, size_t numAttachments, bool depthBuffer);

		RenderTargetPtr createRenderTexture(std::string const& name, size_t width, size_t height, RenderTextureOptions const& options);

		void flushVertexBuffers();

		// Executor contract used to validate MRT shader output locations whenever
		// a program is selected during a graph pass. Zero disables validation.
		void setExpectedGraphColourOutputs(size_t count);

		void beginRenderFlowCapture(RenderPipelineFlowSnapshot* snapshot) noexcept;
		bool endRenderFlowCapture() noexcept;
		bool isRenderFlowCaptureActive() const noexcept;
		void beginRenderFlowPass(GraphPassHandle pass, std::string const& name) noexcept;
		void endRenderFlowPass(GraphPassHandle pass, std::string const& name) noexcept;
		void abortRenderFlowPass() noexcept;
		void failRenderFlowCapture() noexcept;
		void recordRenderFlowBatch(RenderBatchSubmission submission) noexcept;
		void recordRenderFlowStateChanges(std::vector<std::string> changes) noexcept;
		void recordRenderFlowEvent(RenderFlowEventKind kind, std::string const& name,
			GraphImageHandle image = {}, bool enabled = true, std::string const& bypassReason = {},
			std::string const& outputName = {}, bool depth = false,
			std::vector<RenderFlowResourceDesc> inputs = {},
			std::vector<RenderFlowResourceDesc> outputs = {}) noexcept;

		// Clipping
		void pushClipRectangle(ClipRectangle const& clipRect);

		void popClipRectangle();

		void setViewport(int x, int y, size_t width, size_t height);

		void resetViewport();

		//
		// 3d operations
		//

		// Camera
		void pushCameraMatrix();

		void popCameraMatrix();

		glm::mat4 const& getCameraMatrix() const;

		void setCamera3d(glm::vec3 const& position, glm::vec3 const& target, glm::vec3 const& up);

		// Projection
		void pushProjectionMatrix();

		void popProjectionMatrix();

		glm::mat4 const& getProjectionMatrix() const;

		void setProjection3dPerspective(float fov, float nearDist, float farDist);

		glm::mat3 getNormalMatrix() const;

		glm::mat4 const& getModelCameraProjectionMatrix() const;

		// Transforms
		void pushModelMatrix();

		void popModelMatrix();

		glm::mat4 const& getModelMatrix() const;

		void resetTransform();

		// Lights
		void setLightCount(size_t count);

		void setAmbientColour(Colour const& colour);

		void setLight1Position(glm::vec3 const& pos);

		void setLight1Colour(Colour const& colour);

		void setLight2Position(glm::vec3 const& pos);

		void setLight2Colour(Colour const& colour);

		void setPbrAmbientColour(Colour const& colour);

		void setPbrLights(std::vector<PbrLight> const& lights);

		void setActivePbrEnvironment(PbrEnvironmentPtr environment);

		void setActivePipelineSamplerOverrides(std::map<std::string, ResourcePtr> const& overrides);

		// A shadow domain is shared by all pipelines that name it in their
		// RenderPipelineOptions. No domain means no shadow allocations or binds.
		void configureShadowDomain(std::string const& name, ShadowOptions const& options);

		bool hasShadowDomain(std::string const& name) const;

		ShadowOptions const& getShadowDomainOptions(std::string const& name) const;

		RenderTargetPtr getShadowDomainDepthTarget(std::string const& name);

		void ensureShadowDomainResources(std::string const& name);

		void renderShadowDomain(std::string const& name, std::vector<SceneModel3dPtr> const& models);

		void setActiveShadowDomain(std::string const& name);

		static constexpr size_t getMaxPbrLights() { return MaxPbrLights; }

		//
		// 3d operations
		//
		void translateTransform3d(glm::vec3 const& vec);

		void rotateTransform3d(float angle, glm::vec3 const& axis);

		void scaleTransform3d(glm::vec3 const& vec);

		//
		// 2d operations
		//

		// Projection
		void setProjection2dOrthographic();

		// Transforms
		void translateTransform2d(glm::vec2 const& vec);

		void rotateTransform2d(float angle);

		void scaleTransform2d(glm::vec2 const& vec);

		// Rendering
		void addSceneFactory(std::string const& type, SceneFactory factory);

		ScenePtr createScene(std::string const& type);

		void renderScene(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d, std::string const& pipelineName);

		RenderPipelinePtr getOrCreateRenderPipeline(std::string const& name);

		RenderPipelinePtr getOrCreateRenderPipeline(std::string const& name, RenderPipelineOptions const& options);

		RenderPipelinePtr getRenderPipeline(std::string const& name);

		// Removes an obsolete named pipeline after callers have switched away from it.
		bool removeRenderPipeline(std::string const& name);

		RenderInfo const& getRenderInfo() const;

		void startStatsCollection();
		uint64_t getFrameSerial() const;

		RenderInfo const& finishStatsCollection();

		RenderInfo const& getCurrentRenderInfo() const;

		void clearScreen(Colour const& colour);

		void setGamma(float gamma);

		float getGamma() const;
		 
		//
		// 3d rendering
		//
		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend, glm::vec3 const& viewPos);

		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend);

		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend, CameraPtr camera);

		ModelInstance* renderModelBatched(Model const& model, glm::mat4 const& transform, CameraPtr camera);

		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend, glm::vec3 const& viewPos, glm::mat4 const& transform, glm::mat4 const& mcp);

		void renderModelImmediate(Model const& model, bool alphaBlend, glm::vec3 const& viewPos, std::shared_ptr<ModelRenderParams> params);

		void renderModelImmediate(Model const& model, bool alphaBlend, std::shared_ptr<ModelRenderParams> params);

		void renderModelImmediate(Model const& model, bool alphaBlend, std::shared_ptr<ModelRenderParams> params, CameraPtr camera);

		//
		// 2d rendering
		// 
		void renderFullscreenQuad(Texture* texture, BlendMode srcBlend, BlendMode dstBlend, std::shared_ptr<UniformCollection> = nullptr);
		void renderGraphFullscreen(ResourcePtr program, std::vector<std::pair<std::string, Texture*>> const& samplers, UniformCollection const& parameters);

		void renderToneMappedFullscreenQuad(Texture* texture, float exposure, bool useAcesToneMap);

		// Resolves an inspectable colour/depth mip into a display-safe target.
		void renderTextureDiagnostic(RenderTexture* source, RenderTargetPtr const& destination, TextureDiagnosticOptions const& options);

		void renderBloomExtract(Texture* source, float threshold);

		void renderBloomBlur(Texture* source, glm::vec2 const& direction);

		void renderBloomCombine(Texture* scene, Texture* bloom, float intensity);

		void renderQuad(int x, int y, int width, int height, Colour const& colour, bool alphaBlend, bool wireFrame, ResourcePtr texture);

		void renderQuad(int x, int y, int width, int height, Colour const& colour, bool alphaBlend, bool wireFrame);

		void renderText(std::string const& text, int x, int y, Colour const& colour);

		void renderText(std::vector<std::string> const& text, int x, int y, Colour const& colour);

		void renderTextFormatted(std::string const& text, int x, int y);

		void renderTextFormatted(std::vector<std::string> const& text, int x, int y);

		void renderBufferImmediate(int8_t const* vertexData, uint32_t vertexStride, uint32_t numVertices, int8_t* const indexData, uint32_t indexWidth, uint32_t numIndices, std::vector<VertexBufferRenderCommand> const& commands);

		// Override new/delete to force alignment on 16-byte boundary.
		/*
		void* operator new(size_t size)
		{
			return _aligned_malloc(size, 16);
		}

		void operator delete(void* p)
		{
			_aligned_free(p);
		}
		*/
	};

}