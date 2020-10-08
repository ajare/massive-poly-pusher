#pragma once

#include <cstdlib>
#include <map>
#include <deque>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include "mpp/Config.h"
#include "mpp/Caps.h"
#include "mpp/Resource.h"
#include "mpp/RenderTarget.h"
#include "mpp/RenderTexture.h"
#include "mpp/ClipRectangle.h"
#include "mpp/Colour.h"
#include "mpp/Model.h"
#include "mpp/Font.h"
#include "mpp/Texture.h"
#include "mpp/TextureTile.h"
#include "mpp/QuadDefinition.h"
#include "mpp/QuadBatch.h"
#include "mpp/LineBatch.h"
#include "mpp/ModelInstance.h"
#include "mpp/UniformCollection.h"
#include "mpp/Vertex2d.h"
#include "mpp/Logger.h"
#include "mpp/DebugStackWalker.h"

#include <stack>

namespace mpp
{
	class Program;
	class Profiler; // Forward-declared so as to not pollute client apps.
	class ResourceManager;

	enum class BlendMode
	{
		Zero = GL_ZERO,
		One = GL_ONE,
		SrcColour = GL_SRC_COLOR,
		OneMinusSrcColour = GL_ONE_MINUS_SRC_COLOR,
		DstColour = GL_DST_COLOR,
		OneMinusDstColour = GL_ONE_MINUS_DST_COLOR,
		SrcAlpha = GL_SRC_ALPHA,
		OneMinusSrcAlpha = GL_ONE_MINUS_SRC_ALPHA,
		DstAlpha = GL_DST_ALPHA,
		OneMinusDstAlpha = GL_ONE_MINUS_DST_ALPHA
	};

	struct RenderInfo
	{
		int batchCount;
		int programSwitches;
		int textureSwitches;
		int primitivesRendered;
		int fullscreenQuads;
	};

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

	private:

		struct PostProcessEffect
		{
			std::string material;
			UniformCollection uniforms;
			int attachment;
			BlendMode blendSrc, blendDst;
		};

	public:

		enum class TextureTiling
		{
			Stretch,
			Tile
		};

	private:

		typedef std::pair<uint64, MeshInstance*> SortableMeshInstance;
		
	private:

		Logger* mLogger;

		int mWindowWidth, mWindowHeight;

		ResourceManager* mResourceMgr;

		Caps mCaps;

		glm::vec4 mClearColour;

		RenderTargetPtr mRenderTarget;

		std::stack<ClipRectangle> mClipStack;

		RenderTargetPtr mScreen;

		ProjectionType mProjectionType;

		// List of models to render
		std::vector<ModelInstance*> mModelInstances;

		// Default material: 2d with PTC and no texture
		ResourcePtr mDefaultMaterial;

		// Default programs.
		ResourcePtr mDefaultProgram2d, mDefaultProgram3d;

		Program* mwActiveProgram;

		// Internal textures
		ResourcePtr mNoTexture;

		ResourcePtr mInternalFontTexture;

		// Internal font
		Font* mInternalFont;

		ResourcePtr mFontMesh;

		// Texture tiles
		std::map<std::string, TextureTile> mTextureTiles;

		// 3d Transforms
#pragma warning(push)
#pragma warning(disable: 4324)
		alignas(16) glm::mat4 m3dModelCameraProjectionMatrix;
		alignas(16) glm::mat4 m3dCameraMatrix;
		alignas(16) glm::mat4 m3dCameraInverseMatrix;
		alignas(16) glm::mat4 m3dProjectionMatrix;
		alignas(16) glm::mat4 m3dModelMatrix;

		alignas(16) std::stack<glm::mat4> m3dCameraMatrixStack;
		alignas(16) std::stack<glm::mat4> m3dProjectionMatrixStack;
		alignas(16) std::stack<glm::mat4> m3dModelMatrixStack;;
#pragma warning(pop)

		float mFarPlaneDistance;

		// Fullscreen effects
		ResourcePtr mFullscreenQuad;

		std::vector<PostProcessEffect> mPostProcessEffects;

		RenderTargetPtr mSceneTarget, mFullscreenFxTarget, mBlur1Target, mBlur2Target;

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
		DebugStackWalker* mStackWalker;

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

		void setUsedProgram(Program* program);

		Program* getUsedProgram();
		
		void setDefaultTexture();

		int buildTextVertexBuffer(VertexBuffer* buffer, std::string const& text, int& offset, int x, int y);

		int buildColouredTextVertexBuffer(VertexBuffer* buffer, std::string const& text, int& offset, int x, int y);

	public:

		RenderSystem(int windowWidth, int windowHeight);

		virtual ~RenderSystem();

		void _loadCoreResources();

		void _unloadCoreResources();

		int getWindowWidth() const;

		int getWindowHeight() const;

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

		void logMessage(std::string const& message);

		// Render targets and textures
		void setRenderTarget(RenderTargetPtr renderTarget);

		void renderToScreen();

		RenderTargetPtr createRenderTexture(std::string const& name, int width, int height, size_t numAttachments, bool depthBuffer);

		TextureTile const& createTextureTile(std::string const& name, ResourcePtr texture, int offX, int offY, float u0, float v0, float u1, float v1);

		void destroyTextureTile(std::string const& name);

		TextureTile const& getTextureTile(std::string const& name) const;

		void flushVertexBuffers();

		// Clipping
		void pushClipRectangle(ClipRectangle const& clipRect);

		void popClipRectangle();

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
		RenderInfo const& getRenderInfo() const;

		void startScene();

		RenderInfo const& finishScene();

		void clearScreen(Colour const& colour);

		//
		// 3d rendering
		//
		ModelInstance* renderModelBatched(Model const& model, bool alphaBlend, UniformCollection const* uniforms = nullptr, uint32 primitiveCount = -1);

		void renderModelImmediate(Model const& model, bool alphaBlend, UniformCollection const* uniforms = nullptr, uint32 primitiveCount = -1);

		//
		// 2d rendering
		// 
		void addPostEffect(std::string const& material, UniformCollection const& uniforms, int attachment, BlendMode srcBlend, BlendMode dstBlend);

		void renderFullscreenQuad(ResourcePtr material, UniformCollection* uniforms = nullptr);

		void renderFullscreenQuad(RenderTexture* texture, int attachment, BlendMode srcBlend, BlendMode dstBlend, UniformCollection* uniforms = nullptr);

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