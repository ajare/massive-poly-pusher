#pragma once

#include <cstdlib>
#include <map>
#include <deque>
#include <stack>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Caps.h"
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
#include "mpp/QuadDefinition.h"
#include "mpp/QuadBatch.h"
#include "mpp/LineBatch.h"
#include "mpp/ModelRenderParams.h"
#include "mpp/ModelInstance.h"
#include "mpp/UniformCollection.h"
#include "mpp/UniformBuffer.h"
#include "mpp/Vertex2d.h"
#include "mpp/Logger.h"
#include "mpp/SceneFactory.h"

namespace mpp
{
	class Program;
	class Profiler; // Forward-declared so as to not pollute client apps.
	class ResourceManager;

	class _MPPAPI __declspec(align(16)) RenderSystem
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

	private:

		typedef std::pair<uint64_t, MeshInstance*> SortableMeshInstance;
		
	private:

		Logger* mLogger;

		size_t mWindowWidth, mWindowHeight;

		size_t mViewportWidth, mViewportHeight;

		ResourceManager* mResourceMgr;

		Caps mCaps;

		glm::vec4 mClearColour;

		std::map<std::string, RenderPipelinePtr> mPipelines;

		RenderTargetPtr mRenderTarget;

		std::stack<RenderTargetPtr> mRenderTargetStack;

		std::stack<ClipRectangle> mClipStack;

		RenderTargetPtr mScreen;

		ProjectionType mProjectionType;

		// List of models to render
		std::vector<ModelInstance*> mModelInstances;

		// Default material: 2d with PTC and no texture
		ResourcePtr mDefaultMaterial;

		// Default programs
		ResourcePtr mDefaultProgram2d, mDefaultProgram3d;

		ResourcePtr mActiveProgram;

		// Internal textures
		ResourcePtr mNoTexture;

		ResourcePtr mInternalFontTexture;

		// Internal font
		Font* mInternalFont;

		ResourcePtr mFontMesh;

		std::shared_ptr<UniformCollection> mTextUniforms;

		std::shared_ptr<ModelRenderParams> mTextParams;

		// Texture tiles
		std::map<std::string, TextureTile> mTextureTiles;

		// 3d Transforms
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

		// Fullscreen effects
		ResourcePtr mFullscreenQuad;

		RenderTargetPtr mSceneTarget;

		// Text rendering
		ResourcePtr mTextMesh, mColouredTextMesh;

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
		
#ifdef MPP_DEBUG_BUILD

	public:

		struct OpenGLError
		{
			std::string message;
			enum class Severity { Low, Medium, High } severity;
			int count;
		};

	private:

		std::map<std::string, OpenGLError> mOpenGLErrors;
#endif
		
		bool mShowDebugPanel;

		TimeUnit mTimeUnit;

		SizeUnit mSizeUnit;
		
		RenderInfo mRenderInfo;

		// Built-in lights
		UniformBuffer* mLightsBuffer{ nullptr };

		// Scene
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

		void useDefaultProgram();

		void setUsedProgram(ResourcePtr program);

		ResourcePtr getUsedProgram();
		
		void setDefaultTexture();

		int buildTextVertexBuffer(VertexBuffer* buffer, std::string const& text, int& offset, int x, int y);

		int buildColouredTextVertexBuffer(VertexBuffer* buffer, std::string const& text, int& offset, int x, int y);

		void createLightsData();

		void destroyLightsData();

	public:

		RenderSystem(size_t windowWidth, size_t windowHeight);

		virtual ~RenderSystem();

		void _loadCoreResources();

		void _unloadCoreResources();

		size_t getWindowWidth() const;

		size_t getWindowHeight() const;

		float getAspectRatio() const;

		Caps const& getCaps() const;

		void initialise();

		void createCoreResources(ResourceManager* resourceMgr);

		void showDebugPanel(bool show, TimeUnit timeUnit = TimeUnit::Milliseconds, SizeUnit sizeUnit = SizeUnit::Megabytes);

		bool isDebugPanelShown() const;

		void preContextDeletion();

		void postContextCreation(int windowWidth, int windowHeight);

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

		RenderTargetPtr createRenderTexture(std::string const& name, size_t width, size_t height, size_t numAttachments, bool depthBuffer);

		TextureTile const& createTextureTile(std::string const& name, ResourcePtr texture, int offX, int offY, float u0, float v0, float u1, float v1);

		void destroyTextureTile(std::string const& name);

		TextureTile const& getTextureTile(std::string const& name) const;

		void flushVertexBuffers();

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
		ScenePtr createScene(std::string const& type);

		void renderScene(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d, std::string const& pipelineName);

		RenderPipelinePtr createRenderPipeline(std::string const& name);

		RenderPipelinePtr getRenderPipeline(std::string const& name);

		RenderInfo const& getRenderInfo() const;

		void startStatsCollection();

		RenderInfo const& finishStatsCollection();

		void clearScreen(Colour const& colour);
		 
		//
		// 3d rendering
		//
		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend, glm::vec3 const& viewPos);

		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend);

		ModelInstance* renderModelBatched(Model const& model, glm::mat4 const& transform, CameraPtr camera);

		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend, glm::vec3 const& viewPos, glm::mat4 const& transform, glm::mat4 const& mcp);

		void renderModelImmediate(Model const& model, bool alphaBlend, glm::vec3 const& viewPos, std::shared_ptr<ModelRenderParams> params);

		void renderModelImmediate(Model const& model, bool alphaBlend, std::shared_ptr<ModelRenderParams> params);

		//
		// 2d rendering
		// 
		void renderFullscreenQuad(ResourcePtr material, UniformCollection* uniforms = nullptr);

		void renderFullscreenQuad(RenderTexture* texture, int attachment, BlendMode srcBlend, BlendMode dstBlend, std::shared_ptr<UniformCollection> = nullptr);

		void renderQuad(int x, int y, int width, int height, Colour const& colour, bool alphaBlend, bool wireFrame, ResourcePtr texture);

		void renderQuad(int x, int y, int width, int height, Colour const& colour, bool alphaBlend, bool wireFrame);

		void renderText(std::string const& text, int x, int y, Colour const& colour);

		void renderText(std::vector<std::string> const& text, int x, int y, Colour const& colour);

		void renderTextFormatted(std::string const& text, int x, int y);

		// Debug
#ifdef MPP_DEBUG_BUILD
		void addOpenGLError(std::string const& error, OpenGLError::Severity severity);

		void debugStackTrace();
#endif

		// Override new/delete to force alignment on 16-byte boundary.
		void* operator new(size_t size)
		{
			return _aligned_malloc(size, 16);
		}

		void operator delete(void* p)
		{
			_aligned_free(p);
		}
	};

}