#if MPP_PLATFORM == MPP_PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <string>
#include <regex>

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#pragma warning(pop)

#include "utils/StringUtils.h"

#include "mpp/Config.h"
#include "mpp/RenderSystem.h"
#include "mpp/ResourceManager.h"
#include "mpp/Screen.h"
#include "mpp/InternalFont.h"
#include "mpp/Program.h"
#include "mpp/ProgramStream.h"
#include "mpp/Texture.h"
#include "mpp/TextureStream.h"
#include "mpp/Model.h"
#include "mpp/ModelStream.h"
#include "mpp/ProgrammaticModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/ProgrammaticProgramStream.h"
#include "mpp/ProgrammaticTextureStream.h"
#include "mpp/ProgrammaticRenderTextureStream.h"
#include "mpp/RenderTexture.h"
#include "mpp/DefaultShaders.h"
#include "mpp/Profiler.h"
#include "mpp/MeshSortFlags.h"
#include "mpp/MppException.h"
#include "mpp/GLErrorCheck.h"

using namespace std;

namespace mpp
{
	namespace
	{
		struct alignas(16) ShadowFrameData
		{
			glm::mat4 lightViewProjection{ 1.0f };
			glm::vec4 mapTexelSizeAndRadius{ 0.0f };
			glm::vec4 biasAndEnabled{ 0.0f };
		};
		static_assert(offsetof(ShadowFrameData, lightViewProjection) == 0);
		static_assert(offsetof(ShadowFrameData, mapTexelSizeAndRadius) == 64);
		static_assert(offsetof(ShadowFrameData, biasAndEnabled) == 80);
		static_assert(sizeof(ShadowFrameData) == 96);
	}

	/*
	 * Constructor.
	 *
	 */
	RenderSystem::RenderSystem(size_t windowWidth, size_t windowHeight, Logger* logger)
		: ResourceWrangler("RenderSystem")
		, mLogger(logger)
		, mWindowWidth(windowWidth)
		, mWindowHeight(windowHeight)
		, mViewportWidth(windowWidth)
		, mViewportHeight(windowHeight)
		, mResourceMgr(nullptr)
#ifdef MPP_PROFILE_BUILD
		, mProfiler(nullptr)
		, mProfileLines(nullptr)
		, mSamplesToRecord(300)
#endif
		, mRenderTarget(nullptr)
		, mScreen(nullptr)
		, mProjectionType(ProjectionType::Unknown)
		, mModelInstances(nullptr)
		, mMeshInstances(nullptr)
		, mGamma(2.2f)
		, mShowDebugPanel(false)
		, mTimeUnit(TimeUnit::Milliseconds)
		, mSizeUnit(SizeUnit::Megabytes)
		, mInternalFont(nullptr)
		, mInternalVBO(0)
		, mInternalIBO(0)
	{
		// Add scene factories
		mSceneFactories["Default"] = [this](RenderSystem* renderSystem) { return make_shared<Scene>(renderSystem); };

		// Initialise
		initialise();
	}

	/*
	 * Destructor.
	 *
	 */
	RenderSystem::~RenderSystem()
	{
		if (mInternalVBO != 0)
		{
			GL_CHECK(glDeleteBuffers(1, &mInternalVBO));
		}
		if (mInternalIBO != 0)
		{
			GL_CHECK(glDeleteBuffers(1, &mInternalIBO));
		}

		delete mModelInstances;
		delete mMeshInstances;
		delete mInternalFont;
		destroyLightsData();
		destroyPbrLightsData();
		destroyShadowDomains();

		// Deleting the above may have freed up more resources, so sweep once more
		for (auto res : mCoreResources)
		{
			if (!res->isReferenced())
			{
				res->destroy();
			}
		}

#ifdef MPP_PROFILE_BUILD
		delete mProfiler;
		delete mProfileLines;
#endif
	}

	/*
	 * Callback for ARB_debug_output
	 *
	 */
	void APIENTRY debugOutputCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
	                                  GLsizei length, const GLchar* message, GLvoid const* userParam)
	{
		RenderSystem* renderSystem = const_cast<RenderSystem*>((RenderSystem const*)(userParam));

		string msg(message);
		
		// Ignore info messages
		if (msg.find("Buffer detailed info") != string::npos)
		{
			return;
		}

		string strMsg;
		switch (source)
		{
		case GL_DEBUG_SOURCE_API_ARB:
			strMsg = "OpenGL API error :: ";
			break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB:
			strMsg = "OpenGL window error :: ";
			break;

		case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB:
			strMsg = "OpenGL compiler error :: ";
			break;

		case GL_DEBUG_SOURCE_THIRD_PARTY_ARB:
			strMsg = "OpenGL 3rd party error :: ";
			break;

		case GL_DEBUG_SOURCE_APPLICATION_ARB:
			strMsg = "OpenGL application error :: ";
			break;

		case GL_DEBUG_SOURCE_OTHER_ARB:
			strMsg = "OpenGL generic error :: ";
			break;

		default:
			strMsg = "OpenGL error (unknown source) :: ";
			break;
		}

		switch (type)
		{
		case GL_DEBUG_TYPE_ERROR_ARB:
			strMsg += "error :: ";
			break;

		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB:
			strMsg += "deprecated behaviour :: ";
			break;

		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB:
			strMsg += "undefined behaviour :: ";
			break;
			
		case GL_DEBUG_TYPE_PORTABILITY_ARB:
			strMsg += "portability issue :: ";
			break;

		case GL_DEBUG_TYPE_PERFORMANCE_ARB:
			strMsg += "performance issue :: ";
			break;

		case GL_DEBUG_TYPE_OTHER_ARB:
		default:
			strMsg += "unknown type :: ";
			break;
		}

		strMsg += msg;
		renderSystem->errorMessage(strMsg);
	}

	/*
	 * Get window width.
	 *
	 */
	size_t RenderSystem::getWindowWidth() const
	{
		return mWindowWidth;
	}

	/*
	 * Get window height.
	 *
	 */
	size_t RenderSystem::getWindowHeight() const
	{
		return mWindowHeight;
	}

	/*
	 * Get window aspect ratio.
	 *
	 */
	float RenderSystem::getAspectRatio() const
	{
		return (float)mViewportWidth / (float)mViewportHeight;
	}
	
	/*
	 * Get caps.
	 *
	 */
	Caps const& RenderSystem::getCaps() const
	{
		return mCaps;
	}

	/*
	 * Initialise OpenGL.
	 *
	 */
	void RenderSystem::initialise()
	{
		glewExperimental = GL_TRUE; // for GL 3.2 and up.
		GLenum err = glewInit();
		if (err != GLEW_OK)
		{
			string msg = "GLEW initialisation failed.  " + string((char*)glewGetErrorString(err));
			THROW_MPP(msg, __LINE__, __FILE__, __func__);
		}

		checkExtensions();
		checkCaps();

#ifdef MPP_PROFILE_BUILD
		mProfiler = new Profiler();
#endif

		// Pools for rendering objects
		delete mModelInstances;
		mModelInstances = new Pool<ModelInstance>(32);

		delete mMeshInstances;
		mMeshInstances = new Pool<MeshInstance>(64);

		// Set state and display
		setDefaultState();
		setDisplay((int)mWindowWidth, (int)mWindowHeight);
		createLightsData();
		createPbrLightsData();

		// Text settings
		mTextUniforms = make_shared<UniformCollection>();
		mTextUniforms->setUniform("COLOUR", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		mTextParams = make_shared<ModelRenderParams>();
		mTextParams->setModelUniforms(mTextUniforms);
		mTextParams->setModelPointSize(16.0f);

		// Internal buffer objects
		GL_CHECK(glGenBuffers(1, &mInternalVBO));
		GL_CHECK(glGenBuffers(1, &mInternalIBO));
	}

	/*
	 * Toggle debug panel.
	 *
	 */
	void RenderSystem::showDebugPanel(bool show, TimeUnit timeUnit, SizeUnit sizeUnit)
	{
		mShowDebugPanel = show;
		mTimeUnit = timeUnit;
		mSizeUnit = sizeUnit;

#ifdef MPP_PROFILE_BUILD
		mProfileTimeSamples.clear();
#endif
	}

	/*
	 * Is debug panel being rendered?
	 *
	 */	
	bool RenderSystem::isDebugPanelShown() const
	{
		return mShowDebugPanel;
	}

	/*
	 * Check that we have all the extensions that we require.
	 *
	 */
	void RenderSystem::checkExtensions()
	{
		// Check we have the extensions we need.
		if (!GLEW_VERSION_3_2)
		{
			THROW_MPP("OpenGL 3.2 was not found.", __LINE__, __FILE__, __func__);
		}


		int NumberOfExtensions;
		glGetIntegerv(GL_NUM_EXTENSIONS, &NumberOfExtensions);
		for (int i = 0; i < NumberOfExtensions; ++i)
		{
			GLubyte const* ccc = glGetStringi(GL_EXTENSIONS, i);
			string extensionName = string((char const*)ccc);
			if (extensionName == "GL_ARB_debug_output")
			{
#ifdef MPP_DEBUG_BUILD
				glDebugMessageCallbackARB = (PFNGLDEBUGMESSAGECALLBACKARBPROC)wglGetProcAddress("glDebugMessageCallbackARB");

				GL_CHECK(glDebugMessageCallbackARB(&debugOutputCallback, this));
#endif
				infoMessage("GL_ARB_debug_output initialised.");
			}
			else if (extensionName == "GL_EXT_texture_rectangle")
			{
				infoMessage("GL_EXT_texture_rectangle initialised.");
			}
			else if (extensionName == "GLEW_ARB_buffer_storage")
			{
				infoMessage("GLEW_ARB_buffer_storage initialised.");
			}
			else if (extensionName == "GLEW_ARB_map_buffer_range")
			{
				infoMessage("GLEW_ARB_map_buffer_range initialised.");
			}
		}

		/*
		if (GLEW_NVX_gpu_memory_info)
		{
			logMessage("Found 'NVX_gpu_memory_info' extension.");
		}
		*/
	}

	/*
	 *  Check capabilities, to determine how to render things.
	 *
	 */
	void RenderSystem::checkCaps()
	{
		// Get versions
		string glVersion((char*)glGetString(GL_VERSION));
		string glslVersion((char*)glGetString(GL_SHADING_LANGUAGE_VERSION));
		string glRenderer((char*)glGetString(GL_RENDERER));
		string vendorInfo((char*)glGetString(GL_VENDOR));

		// GL_VERSION
		regex glVersionRe(R"(^(\d+)\.(\d+)\.?(\d+)?\s?(.*)?)");
		smatch matches;

		if (regex_search(glVersion, matches, glVersionRe))
		{
			string vMajor = matches[1];
			string vMinor = matches[2];
			string vRelease = matches[3];
			string vendorSpecific = matches[4];

			mCaps.glVersionMajor = utils::StringUtils::parseInt(vMajor);
			mCaps.glVersionMinor = utils::StringUtils::parseInt(vMinor);
		}
		else 
		{
			THROW_MPP("Could not parse GL_VERSION: " + glVersion, __LINE__, __FILE__, __func__);
		}

		// GL_SHADING_LANGUAGE_VERSION
		if (regex_search(glslVersion, matches, glVersionRe))
		{
			string vMajor = matches[1];
			string vMinor = matches[2];
			string vRelease = matches[3];
			string vendorSpecific = matches[4];

			mCaps.glslVersionMajor = utils::StringUtils::parseInt(vMajor);
			mCaps.glslVersionMinor = utils::StringUtils::parseInt(vMinor);
		}
		else
		{
			THROW_MPP("Could not parse GL_SHADING_LANGUAGE_VERSION: " + glslVersion, __LINE__, __FILE__, __func__);
		}

		infoMessage("GL version: " + glVersion);
		infoMessage("GLSL version: " + glslVersion);
		infoMessage("Renderer: " + glRenderer);
		infoMessage("Vendor: " + vendorInfo);

		// Get point size render range
		GLfloat sizeRange[2] = { 0.0f, 0.0f };
		GL_CHECK(glGetFloatv(GL_SMOOTH_POINT_SIZE_RANGE, sizeRange));

		mCaps.pointSizeRange[0] = sizeRange[0];
		mCaps.pointSizeRange[1] = sizeRange[1];

		// Get aliased line width range
		GLfloat lineRange[2] = { 0.0f, 0.0f };
		GL_CHECK(glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, lineRange));

		mCaps.aliasedLineWidthRange[0] = lineRange[0];
		mCaps.aliasedLineWidthRange[1] = lineRange[1];

		// Get texture info
		GLint maxTextureSize;
		GLint maxRectTextureSize = 0;

		GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize));

		if (glewIsSupported("GL_EXT_texture_rectangle"))
		{
			GL_CHECK(glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &maxRectTextureSize));
		}

		mCaps.maxTextureSize = maxTextureSize;
		mCaps.maxRectTextureSize = maxRectTextureSize;
		
		// Get depth range
		GLfloat depthRange[2] = { 0.0f, 0.0f };
		glGetFloatv(GL_DEPTH_RANGE, depthRange);

		mCaps.depthRange[0] = depthRange[0];
		mCaps.depthRange[1] = depthRange[1];

		// Primitive counts
		GLint maxElements, maxVertices;
		GL_CHECK(glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &maxElements));
		GL_CHECK(glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &maxVertices));

		mCaps.maxRecommendedElements = maxElements;
		mCaps.maxRecommendedVertices = maxVertices;

		// Streaming geometry
		// ARB_buffer_storage && ARB_map_buffer_range
		mCaps.streamingGeometry = GLEW_ARB_buffer_storage && GLEW_ARB_map_buffer_range;

		// Filtering
		float maxAnisotropy;
		GL_CHECK(glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy));

		mCaps.maxAnisotropy = maxAnisotropy;

		// Uniform limits
		int maxUniforms;
		GL_CHECK(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &maxUniforms));
		mCaps.maxVertexShaderUniforms = (uint32_t)maxUniforms;

		GL_CHECK(glGetIntegerv(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS, &maxUniforms));
		mCaps.maxGeometryShaderUniforms = (uint32_t)maxUniforms;

		GL_CHECK(glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &maxUniforms));
		mCaps.maxFragmentShaderUniforms = (uint32_t)maxUniforms;

		// Texture limits
		int maxTextureUnits;
		GL_CHECK(glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxTextureUnits));
		mCaps.maxVertexTextureUnits = (uint32_t)maxUniforms;

		GL_CHECK(glGetIntegerv(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, &maxTextureUnits));
		mCaps.maxGeometryTextureUnits = (uint32_t)maxUniforms;

		GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureUnits));
		mCaps.maxFragmentTextureUnits = (uint32_t)maxUniforms;

		// Print caps
		infoMessage(STR_FORMAT("Supported point size range: {} to {}", mCaps.pointSizeRange[0], mCaps.pointSizeRange[1]));
		infoMessage(STR_FORMAT("Supported aliased line width range: {} to {}", mCaps.aliasedLineWidthRange[0], mCaps.aliasedLineWidthRange[1]));
		infoMessage(STR_FORMAT("Supported square texture size: {}x{}", mCaps.maxTextureSize, mCaps.maxTextureSize));
		infoMessage(STR_FORMAT("Supported non-square texture size: {}x{}", mCaps.maxRectTextureSize, mCaps.maxRectTextureSize));
		infoMessage(STR_FORMAT("Depth range: {} to {}", mCaps.depthRange[0], mCaps.depthRange[1]));
		infoMessage(STR_FORMAT("Max anisotropy: {}", mCaps.maxAnisotropy));
		infoMessage(STR_FORMAT("Max recommended elements: {}", mCaps.maxRecommendedElements));
		infoMessage(STR_FORMAT("Max recommended vertices: {}", mCaps.maxRecommendedVertices));
		infoMessage(STR_FORMAT("Streaming geometry: {}", mCaps.streamingGeometry ? "yes" : "no"));

		infoMessage(STR_FORMAT("Max vertex shader uniforms: {}", mCaps.maxVertexShaderUniforms));
		infoMessage(STR_FORMAT("Max geometry shader uniforms: {}", mCaps.maxGeometryShaderUniforms));
		infoMessage(STR_FORMAT("Max fragment shader uniforms: {}", mCaps.maxFragmentShaderUniforms));

		infoMessage(STR_FORMAT("Max vertex texture units: {}", mCaps.maxVertexTextureUnits));
		infoMessage(STR_FORMAT("Max geometry texture units: {}", mCaps.maxGeometryTextureUnits));
		infoMessage(STR_FORMAT("Max fragment texture units: {}", mCaps.maxFragmentTextureUnits));
	}

	void RenderSystem::addCoreResource(ResourcePtr resource, bool load)
	{
		resource->acquire(this);
		if (load)
		{
			resource->load();
		}

		mCoreResources.push_back(resource);
	}

	/*
	 * Create core resources which the RenderSystem needs to operate on,
	 * outside of the client-specific resources.
	 *
	 */
	void RenderSystem::createCoreResources(ResourceManager* resourceMgr)
	{
		mResourceMgr = resourceMgr;

		// Default 3d program
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Normal3, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShader3dTemplate);
			parser->setFragmentSource(FragmentShader3dTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			mDefaultProgram3d = resourceMgr->declareResource("__mpp_p3d_tris_p3n3t2c4__", ResourceStreamPtr(ps)).first;
			addCoreResource(mDefaultProgram3d, true);
		}

		// Generic shadow caster program: position is the only material-independent
		// vertex requirement, so PBR and legacy meshes use the same depth pass.
		{
			mesh::MeshSpecification meshSpec;
			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position3, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();
			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderShadowDepthTemplate);
			parser->setFragmentSource(FragmentShaderShadowDepthTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			mShadowDepthProgram = resourceMgr->declareResource("__mpp_p3d_shadow_depth__", ResourceStreamPtr(ps)).first;
			addCoreResource(mShadowDepthProgram, true);
		}
		createShadowDisabledFrameBuffer();

		// 2d fullscreen program
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderFullscreenTemplate);
			parser->setFragmentSource(FragmentShaderFullscreenTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			mFullscreenProgram = resourceMgr->declareResource("__mpp_p2d_fullscreen__", ResourceStreamPtr(ps)).first;
			addCoreResource(mFullscreenProgram, true);
		}

		// HDR tone-map program used by the opt-in PBR preview pipeline.
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();
			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderFullscreenTemplate);
			parser->setFragmentSource(FragmentShaderToneMapTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			mToneMapProgram = resourceMgr->declareResource("__mpp_p2d_tonemap__", ResourceStreamPtr(ps)).first;
			addCoreResource(mToneMapProgram, true);
		}

		// Internal text programs
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			ps->setAttribs({ "Points" });

			auto res = resourceMgr->declareResource("__mpp_p2d_points_text__", ResourceStreamPtr(ps)).first;
			addCoreResource(res, false);
		}
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);

			auto res = resourceMgr->declareResource("__mpp_p2d_tris_text__", ResourceStreamPtr(ps)).first;
			addCoreResource(res, true);
		}
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);
			//layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			ps->setAttribs({ "Points", "Colours" });

			auto res = resourceMgr->declareResource("__mpp_p2d_points_text_coloured__", ResourceStreamPtr(ps)).first;
			addCoreResource(res, false);
		}
		{
			mesh::MeshSpecification meshSpec;

			auto layout = meshSpec.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			//layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, false);

			auto parser = make_shared<program::Parser>();

			parser->setMeshSpecification(meshSpec);
			parser->setVertexSource(VertexShaderTextTemplate);
			parser->setFragmentSource(FragmentShaderTextTemplate);

			auto ps = new ProgrammaticProgramStream(resourceMgr);
			ps->setParser(parser);
			ps->setAttribs({ "Colours" });

			auto res = resourceMgr->declareResource("__mpp_p2d_tris_text_coloured__", ResourceStreamPtr(ps)).first;
			addCoreResource(res, true);
		}

		// Default texture
		auto blankStream = new ProgrammaticTextureStream(resourceMgr);
		blankStream->setTarget(TextureTarget::Texture2D);
		blankStream->setData([](string const& id)
		{
			TextureData data;

			data.width = 1;
			data.height = 1;
			data.bitsPerPixel = 24;
			data.dataType = GL_UNSIGNED_BYTE;
			data.pixelFormat = GL_RGB;

			size_t dataSize = (data.width * data.height * data.bitsPerPixel / 8);

			data.data = new uint8_t[dataSize];
			memset(data.data, 255, dataSize);

			return data;
		});

		blankStream->setFiltering(TextureParams::MinFilter::Nearest, TextureParams::MagFilter::Nearest);
		mNoTexture = resourceMgr->declareResource("__mpp_tex_none__", ResourceStreamPtr(blankStream)).first;
		addCoreResource(mNoTexture, true);

		// Standard PBR fallback maps. PBR materials can omit any texture map;
		// Material resolves the corresponding canonical sampler to these values.
		auto addPbrFallbackTexture = [this, resourceMgr](string const& name, uint8_t red, uint8_t green, uint8_t blue, TextureColourSpace colourSpace)
		{
			auto stream = new ProgrammaticTextureStream(resourceMgr);
			stream->setTarget(TextureTarget::Texture2D);
			stream->setColourSpace(colourSpace);
			stream->setData([red, green, blue](string const&)
			{
				TextureData data;
				data.width = 1;
				data.height = 1;
				data.bitsPerPixel = 24;
				data.dataType = GL_UNSIGNED_BYTE;
				data.pixelFormat = GL_RGB;
				data.data = new uint8_t[3]{ red, green, blue };
				return data;
			});
			auto texture = resourceMgr->declareResource(name, ResourceStreamPtr(stream)).first;
			addCoreResource(texture, true);
		};
		addPbrFallbackTexture("__mpp_tex_pbr_white__", 255, 255, 255, TextureColourSpace::Srgb);
		addPbrFallbackTexture("__mpp_tex_pbr_black__", 0, 0, 0, TextureColourSpace::Srgb);
		addPbrFallbackTexture("__mpp_tex_pbr_normal__", 128, 128, 255, TextureColourSpace::Linear);
		addPbrFallbackTexture("__mpp_tex_pbr_metallic_roughness__", 0, 255, 255, TextureColourSpace::Linear);

		// Internal font texture
		auto ts = new ProgrammaticTextureStream(resourceMgr);
		ts->setTarget(TextureTarget::Texture2D);
		ts->setData([](string const& id)
		{
			InternalFont internalFont;
			TextureData data;

			data.width = internalFont.getWidth();
			data.height = internalFont.getHeight();
			data.bitsPerPixel = 32;
			data.dataType = GL_UNSIGNED_BYTE;
			data.pixelFormat = GL_RGBA;

			size_t dataSize = (data.width * data.height * data.bitsPerPixel / 8);

			data.data = new uint8_t[dataSize];
			memcpy(data.data, (uint8_t const*)internalFont.getData(), dataSize);

			return data;
		});

		ts->setFiltering(TextureParams::MinFilter::Nearest, TextureParams::MagFilter::Nearest);

		mInternalFontTexture = resourceMgr->declareResource("__mpp_tex_internalfont__", ResourceStreamPtr(ts)).first;
		addCoreResource(mInternalFontTexture, true);

		mInternalFont = new Font(mInternalFontTexture);

		// Default 2d program
		mesh::MeshSpecification spec2d(mesh::Primitive::Type::Triangles);
		auto layout = spec2d.createVertexBufferAttributeLayout(false);
		layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

		mDefaultProgram2d = resourceMgr->getDefault2dProgram(spec2d, 0, true);
		addCoreResource(mDefaultProgram2d, false);

		// Default material
		auto defaultMatStream = new ProgrammaticMaterialStream(mResourceMgr);
		
		defaultMatStream->setProgram(mDefaultProgram2d->getName());
		defaultMatStream->setTexture("TEX1", "__mpp_tex_none__");
		mDefaultMaterial = resourceMgr->declareResource("__mpp_mat_default__", mpp::ResourceStreamPtr(defaultMatStream)).first;
		addCoreResource(mDefaultMaterial, true);

		// Internal 2d program
		{
			mesh::MeshSpecification spec2dinternal(mesh::Primitive::Type::Triangles);
			spec2dinternal.setIndexedVertices(true);

			auto layout = spec2dinternal.createVertexBufferAttributeLayout(false);
			layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

			mInternalProgram2d = resourceMgr->getDefault2dProgram(spec2dinternal, MPP_PROGRAM_TAGS_TEXTURE, true);
			addCoreResource(mInternalProgram2d, true);
		}

		// Internal 2d material
		{
			auto internalMatStream = new ProgrammaticMaterialStream(mResourceMgr);

			internalMatStream->setProgram(mInternalProgram2d->getName());
			internalMatStream->setTexture("TEX1", "__mpp_tex_none__");
			mInternalMaterial = resourceMgr->declareResource("__mpp_mat_internal__", mpp::ResourceStreamPtr(internalMatStream)).first;
			addCoreResource(mInternalMaterial, true);
		}

		// Internal font
		bool textAsPoints = mCaps.pointSizeRange[1] >= 16.0f;
		ProgrammaticMaterialStream* textMatStream = new ProgrammaticMaterialStream(mResourceMgr);

		textMatStream->setProgram(textAsPoints ? "__mpp_p2d_points_text__" : "__mpp_p2d_tris_text__");
		textMatStream->setUniform("COLOUR", glm::vec4(1, 1, 1, 1));
		textMatStream->setTexture("TEX1", "__mpp_tex_internalfont__");
		auto res = resourceMgr->declareResource("__mpp_mat_text_pt__", mpp::ResourceStreamPtr(textMatStream)).first;
		addCoreResource(res, true);

		ProgrammaticMaterialStream* textMatStreamColoured = new ProgrammaticMaterialStream(mResourceMgr);
		textMatStreamColoured->setProgram(textAsPoints ? "__mpp_p2d_points_text_coloured__" : "__mpp_p2d_tris_text_coloured__");
		textMatStreamColoured->setUniform("COLOUR", glm::vec4(1, 1, 1, 1));
		textMatStreamColoured->setTexture("TEX1", "__mpp_tex_internalfont__");
		res = resourceMgr->declareResource("__mpp_mat_text_ptc__", mpp::ResourceStreamPtr(textMatStreamColoured)).first;
		addCoreResource(res, true);

		int fontTextureWidth = (int)((Texture&)*mInternalFontTexture).getWidth();
		int fontTextureHeight = (int)((Texture&)*mInternalFontTexture).getHeight();
		int glyphWidth = fontTextureWidth / 16;
		int glyphHeight = fontTextureHeight / 16;
		for (int i = 0; i < 256; ++i)
		{
			int kern = -8;
			if (i == 'f') kern = -9;
			else if (i == 'i') kern = -10;
			else if (i == 'j') kern = -10;
			else if (i == 'r') kern = -10;

			mInternalFont->setGlyph(
				(uint8_t)i,
				(i % 16) * 16,
				fontTextureHeight - 16 - (i / 16) * 16,
				glyphWidth,
				glyphHeight,
				kern,
				0);
		}

		// Create font mesh
		auto textStream = new ProgrammaticModelStream(mResourceMgr);

		if (textAsPoints)
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Points);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout(false);

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);

			int textMesh = (int)textStream->createMesh("text-mesh", textSpec, "__mpp_mat_text_pt__", 32, 16.0f);

			for (uint32_t i = 0; i < MaxTextGlyphs; ++i)
			{
				textStream->addVertexData(textMesh, mesh::VertexData(textSpec, 1).f32(0.0f).f32(0.0f).f32(0.0f).f32(0.0f).f32(1.0f).f32(1.0f));
			}
		}
		else
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Triangles);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout(false);

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			int textMesh = (int)textStream->createMesh("text-mesh", textSpec, "__mpp_mat_text_pt__", 32, -1.0f);

			for (uint32_t i = 0; i < MaxTextGlyphs * 6; ++i)
			{
				textStream->addVertexData(textMesh, mesh::VertexData(textSpec, 1).f32(0.0f).f32(0.0f).f32(0.0f).f32(0.0f));
			}
		}

		auto textStreamPtr = ResourceStreamPtr(textStream);
		mTextMesh = resourceMgr->declareResource("__mpp_internal_text_mesh__", textStreamPtr).first;
		addCoreResource(mTextMesh, true);

		// Coloured font mesh
		textStream = new ProgrammaticModelStream(mResourceMgr);

		if (textAsPoints)
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Points);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout(false);

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);
			//attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);
			attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, false);

			int textMesh = (int)textStream->createMesh("text-mesh", textSpec, "__mpp_mat_text_ptc__", 32, 16.0f);

			for (uint32_t i = 0; i < MaxTextGlyphs; ++i)
			{
				textStream->addVertexData(textMesh, mesh::VertexData(textSpec, 1).f32(0.0f).f32(0.0f).f32(0.0f).f32(0.0f)
					.f32(1.0f).f32(1.0f).u8(255).u8(255).u8(255).u8(255));
			}
		}
		else
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Triangles);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout(false);

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			//attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);
			attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, false);

			int textMesh = (int)textStream->createMesh("text-mesh", textSpec, "__mpp_mat_text_ptc__", 32, -1.0f);

			for (uint32_t i = 0; i < MaxTextGlyphs * 6; ++i)
			{
				textStream->addVertexData(textMesh, mesh::VertexData(textSpec, 1).f32(0.0f).f32(0.0f).f32(0.0f).f32(0.0f)
					.f32(1.0f).f32(1.0f).u8(255).u8(255).u8(255).u8(255));
			}
		}

		textStreamPtr = ResourceStreamPtr(textStream);
		mColouredTextMesh = resourceMgr->declareResource("__mpp_internal_coloured_text_mesh__", textStreamPtr).first;
		addCoreResource(mColouredTextMesh, true);

		// Fullscreen mesh
		auto quadStream = new ProgrammaticModelStream(mResourceMgr);
		mesh::MeshSpecification quadSpec = mesh::MeshSpecification(mesh::Primitive::Type::Triangles);

		auto attribLayout = quadSpec.createVertexBufferAttributeLayout(false);
		attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
		quadSpec.setStorageType(mesh::VertexBufferStorageType::Static);
		quadSpec.setIndexedVertices(false);

		auto fullscreenProgram = resourceMgr->getDefault2dProgram(quadSpec, 0, true);
		auto quadMesh = quadStream->createMesh("fullscreen-quad", quadSpec, fullscreenProgram->getName(), 32, -1.0f);

		quadStream->addVertexData(quadMesh, mesh::VertexData(quadSpec, 1).f32(0.0f).f32(0.0f).f32(0.0f).f32(0.0f));
		quadStream->addVertexData(quadMesh, mesh::VertexData(quadSpec, 1).f32((float)mWindowWidth).f32(0.0f).f32(1.0f).f32(0.0f));
		quadStream->addVertexData(quadMesh, mesh::VertexData(quadSpec, 1).f32((float)mWindowWidth).f32((float)mWindowHeight).f32(1.0f).f32(1.0f));
		quadStream->addVertexData(quadMesh, mesh::VertexData(quadSpec, 1).f32((float)mWindowWidth).f32((float)mWindowHeight).f32(1.0f).f32(1.0f));
		quadStream->addVertexData(quadMesh, mesh::VertexData(quadSpec, 1).f32(0.0f).f32((float)mWindowHeight).f32(0.0f).f32(1.0f));
		quadStream->addVertexData(quadMesh, mesh::VertexData(quadSpec, 1).f32(0.0f).f32(0.0f).f32(0.0f).f32(0.0f));

		mFullscreenQuad = resourceMgr->declareResource("__mpp_mesh_fullscreen_quad__", ResourceStreamPtr(quadStream)).first;
		addCoreResource(mFullscreenQuad, true);

		// Render targets are owned by render passes/pipelines. Do not create a
		// duplicate global scene target here.

		// Set none as active
		mActiveProgram.reset();

		// Profile graph
#ifdef MPP_PROFILE_BUILD
		mProfileLines = new LineBatch(
			"ProfileLines",
			Batch::ColourOptions::FloatRGBA,
			false,
			mSamplesToRecord * 5 + 2, // 5 seconds + 2 extra for graph
			this,
			mResourceMgr);

		mProfileLines->load();
#endif
	}

	void RenderSystem::destroyCoreResources()
	{
		// Release
		for (auto res : mCoreResources)
		{
			res->release(this);

		}

		// Destroy what we can
		for (auto res : mCoreResources)
		{
			if (!res->isReferenced())
			{
				res->destroy();
			}
		}
	}


	/*
	 * Use the default program.
	 *
	 */
	void RenderSystem::useDefaultProgram()
	{
		switch (mProjectionType)
		{
		case ProjectionType::Perspective3D:
			setUsedProgram(mDefaultProgram3d);
			break;

		case ProjectionType::Ortho2D:
			setUsedProgram(mDefaultProgram2d);
			break;

		default:
			THROW_MPP("Unsupported projection.", __LINE__, __FILE__, __func__);
		}
	}

	/*
	 * Set the used program.  This should only be called
	 * after GL has accepted the program as active.
	 *
	 */
	void RenderSystem::setUsedProgram(ResourcePtr program)
	{
		if (program == mActiveProgram)
		{
			return;
		}

		static_cast<Program*>(program.get())->bind();
		mActiveProgram = program;
	}

	/*
	 * Get used program.
	 *
	 */
	ResourcePtr RenderSystem::getUsedProgram()
	{
		return mActiveProgram;
	}

	/*
	 * Use the default texture (no texture).
	 *
	 */
	void RenderSystem::setDefaultTexture()
	{
		GL_CHECK(glActiveTexture(GL_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
	}

	/*
	* Set up basic OpenGL state.
	*
	*/
	void RenderSystem::setDefaultState()
	{
		GL_CHECK(glDisable(GL_SCISSOR_TEST));
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glDepthFunc(GL_LESS));

		GL_CHECK(glEnable(GL_PROGRAM_POINT_SIZE));

		// Set matrices to identity
		m3dCameraMatrix = glm::mat4();
		m3dProjectionMatrix = glm::mat4();
		m3dModelMatrix = glm::mat4();
		m3dModelCameraProjectionMatrix = glm::mat4();
	}

	/*
	 * Set up the display/viewport.
	 *
	 */
	void RenderSystem::setDisplay(int width, int height)
	{
		if (mScreen)
		{
			mScreen.reset();
		}

		mWindowWidth = (size_t)width;
		mWindowHeight = (size_t)height;
		mScreen = RenderTargetPtr(new Screen(width, height));
		setRenderTarget(mScreen);

		for (auto const& [name, pipeline] : mPipelines)
		{
			pipeline->resize(mWindowWidth, mWindowHeight);
		}

		if (mFullscreenQuad)
		{
			auto mesh = static_cast<Model*>(mFullscreenQuad.get())->getMesh(0);
			auto buffer = mesh->getVertexBuffer(0);
			auto& data = buffer->getBufferData();
			const float vertices[] =
			{
				0.0f, 0.0f, 0.0f, 0.0f,
				(float)mWindowWidth, 0.0f, 1.0f, 0.0f,
				(float)mWindowWidth, (float)mWindowHeight, 1.0f, 1.0f,
				(float)mWindowWidth, (float)mWindowHeight, 1.0f, 1.0f,
				0.0f, (float)mWindowHeight, 0.0f, 1.0f,
				0.0f, 0.0f, 0.0f, 0.0f
			};
			memcpy(data.data(), vertices, sizeof(vertices));
			buffer->mapBufferData(6);
		}
	}

	/*
	 * Write a message to logfile.
	 *
	 */
	void RenderSystem::debugMessage(string const& message)
	{
		mLogger->debug(message);
	}

	void RenderSystem::infoMessage(string const& message)
	{
		mLogger->info(message);
	}

	void RenderSystem::warnMessage(string const& message)
	{
		mLogger->warn(message);
	}

	void RenderSystem::errorMessage(string const& message)
	{
		mLogger->error(message);
	}

	/*
	 * Set the current render target.
	 *
	 */
	void RenderSystem::setRenderTarget(RenderTargetPtr renderTarget)
	{
		flushVertexBuffers();

		if (mRenderTarget.get())
		{
			mRenderTarget->deactivate();
		}

		mRenderTarget = renderTarget;
		mRenderTarget->activate();
	}

	void RenderSystem::pushRenderTarget(RenderTargetPtr renderTarget)
	{
		mRenderTargetStack.push(mRenderTarget);
		setRenderTarget(renderTarget);
	}

	void RenderSystem::popRenderTarget()
	{
		auto renderTarget = mRenderTargetStack.top();
		mRenderTargetStack.pop();

		setRenderTarget(renderTarget);
	}

	/*
	 * Set screen as target.
	 *
	 */
	void RenderSystem::renderToScreen()
	{
		flushVertexBuffers();

		if (mRenderTarget.get())
		{
			mRenderTarget->deactivate();
		}

		mRenderTarget = mScreen;
		mScreen->activate();
	}

	/*
	 * Create a new render texture.
	 *
	 */
	RenderTargetPtr RenderSystem::createRenderTexture(string const& name, size_t width, size_t height, size_t numAttachments, bool depthBuffer)
	{
		RenderTextureOptions options;
		options.numAttachments = numAttachments;
		options.depthAttachment = depthBuffer ? RenderTextureDepthAttachment::DepthRenderbuffer : RenderTextureDepthAttachment::None;
		return createRenderTexture(name, width, height, options);
	}

	RenderTargetPtr RenderSystem::createRenderTexture(string const& name, size_t width, size_t height, RenderTextureOptions const& options)
	{
		auto rtStream = new ProgrammaticRenderTextureStream(mResourceMgr);

		rtStream->setTarget(TextureTarget::Texture2D);
		rtStream->setInternalFormat(options.colourType, options.colourNormalised, options.colourBitSize, options.colourChannels);
		rtStream->setParams(options.params);
		rtStream->setWidth(width);
		rtStream->setHeight(height);
		rtStream->setDepthAttachment(options.depthAttachment);
		rtStream->setDepthParams(options.depthParams);
		rtStream->setNumAttachments(options.numAttachments);

		auto rt = new RenderTexture(name, this, mResourceMgr, ResourceStreamPtr(rtStream));
		rt->load();

		return RenderTargetPtr(rt);
	}

	/*
	 * Clip the screen.
	 *
	 */
	void RenderSystem::pushClipRectangle(ClipRectangle const& clipRect)
	{
		ClipRectangle cr = clipRect;

		if (!mClipStack.empty())
		{
			cr = clipRect.intersect(mClipStack.top());
		}

		mClipStack.push(cr);
		GL_CHECK(glScissor(cr.x, cr.y, cr.width, cr.height));
	}

	/*
	 * Un-clip the screen.
	 *
	 */
	void RenderSystem::popClipRectangle()
	{
		assert(!mClipStack.empty() && "RenderSystem::popClipRectangle() 'mClipStack' is empty.");
		assert(mRenderTarget && "RenderSystem::popClipRectangle() No active RenderTarget.");
		
		mClipStack.pop();
		if (mClipStack.empty())
		{
			// Set to target size.
			GL_CHECK(glScissor(0, 0, (GLsizei)mRenderTarget->getWidth(), (GLsizei)mRenderTarget->getHeight()));
		}
		else
		{
			ClipRectangle const& cr = mClipStack.top();
			GL_CHECK(glScissor(cr.x, cr.y, (GLsizei)cr.width, (GLsizei)cr.height));
		}
	}

	void RenderSystem::setViewport(int x, int y, size_t width, size_t height)
	{
		mViewportWidth = width;
		mViewportHeight = height;
		GL_CHECK(glViewport(x, y, (GLsizei)mViewportWidth, (GLsizei)mViewportHeight));
		GL_CHECK(glScissor(x, y, (GLsizei)mViewportWidth, (GLsizei)mViewportHeight));
	}

	void RenderSystem::resetViewport()
	{
		setViewport(0, 0, mRenderTarget->getWidth(), mRenderTarget->getHeight());
	}

	/*
	 * Reset the model matrix to identity.
	 *
	 */
	void RenderSystem::resetTransform()
	{
		m3dModelMatrix = glm::mat4();

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Translate the model matrix in 3d.
	 *
	 */
	void RenderSystem::translateTransform3d(glm::vec3 const& vec)
	{
		m3dModelMatrix = glm::translate(m3dModelMatrix, vec);

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Rotate the model matrix in 3d.
	 *
	 */
	void RenderSystem::rotateTransform3d(float angle, glm::vec3 const& axis)
	{
		m3dModelMatrix = glm::rotate(m3dModelMatrix, glm::radians(angle), axis);

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Scale the model matrix in 3d.
	 *
	 */
	void RenderSystem::scaleTransform3d(glm::vec3 const& vec)
	{
		m3dModelMatrix = glm::scale(m3dModelMatrix, vec);

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Push the current camera matrix.
	 *
	 */
	void RenderSystem::pushCameraMatrix()
	{
		m3dCameraMatrixStack.push(m3dCameraMatrix);
	}

	/*
	 * Pop the 3d camera matrix.
	 *
	 */
	void RenderSystem::popCameraMatrix()
	{
		assert(!m3dCameraMatrixStack.empty() && "RenderSystem::popCameraMatrix3d() 'm3dCameraMatrixStack' is empty.");

		m3dCameraMatrix = m3dCameraMatrixStack.top();
		m3dCameraMatrixStack.pop();

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Get the current 3d camera matrix.
	 *
	 */
	glm::mat4 const& RenderSystem::getCameraMatrix() const
	{
		return m3dCameraMatrix;
	}

	/*
	 * Set 3d camera.
	 *
	 */
	void RenderSystem::setCamera3d(glm::vec3 const& position, glm::vec3 const& target, glm::vec3 const& up)
	{
		m3dCameraMatrix = glm::lookAt(position, target, up);

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Push the current 3d projection matrix.
	 *
	 */
	void RenderSystem::pushProjectionMatrix()
	{
		m3dProjectionMatrixStack.push(m3dProjectionMatrix);
	}

	/*
	 * Pop the 3d projection matrix.
	 *
	 */
	void RenderSystem::popProjectionMatrix()
	{
		assert(!m3dProjectionMatrixStack.empty() && "RenderSystem::popProjectionMatrix3d() 'm3dProjectionMatrixStack' is empty.");

		m3dProjectionMatrix = m3dProjectionMatrixStack.top();
		m3dProjectionMatrixStack.pop();

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}
	
	/*
	 * Get the current 3d projection matrix.
	 *
	 */
	glm::mat4 const& RenderSystem::getProjectionMatrix() const
	{
		return m3dProjectionMatrix;
	}

	/*
	 * Set 3d projection.
	 *
	 */
	void RenderSystem::setProjection3dPerspective(float fov, float nearDist, float farDist)
	{
		flushVertexBuffers();

		GL_CHECK(glEnable(GL_DEPTH_TEST));

		mProjectionType = ProjectionType::Perspective3D;
		mFarPlaneDistance = farDist;

		// Reset camera and model matrices
		m3dCameraMatrix = glm::mat4();
		m3dModelMatrix = glm::mat4();

		// Recalculate combined matrix
		m3dProjectionMatrix = glm::perspective(glm::radians(fov), getAspectRatio(), nearDist, farDist);
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Push the current model matrix.
	 *
	 */
	void RenderSystem::pushModelMatrix()
	{
		m3dModelMatrixStack.push(m3dModelMatrix);
	}

	/*
	 * Pop the 3d model matrix.
	 *
	 */
	void RenderSystem::popModelMatrix()
	{
		assert(!m3dModelMatrixStack.empty() && "RenderSystem::popModelMatrix3d() 'm3dModelMatrixStack' is empty.");

		m3dModelMatrix = m3dModelMatrixStack.top();
		m3dModelMatrixStack.pop();

		// Recalculate combined matrix
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Get the current 3d model matrix.
	 *
	 */
	glm::mat4 const& RenderSystem::getModelMatrix() const
	{
		return m3dModelMatrix;
	}

	/*
	 * Get the current normal matrix.
	 *
	 */
	glm::mat3 RenderSystem::getNormalMatrix() const
	{
		return glm::transpose(glm::inverse(glm::mat3(m3dModelMatrix)));
	}

	/*
	 * Get the combined model-camera-projection matrix.
	 *
	 */
	glm::mat4 const& RenderSystem::getModelCameraProjectionMatrix() const
	{
		return m3dModelCameraProjectionMatrix;
	}

	/*
	 * Set 2d projection.
	 *
	 */
	void RenderSystem::setProjection2dOrthographic()
	{
		flushVertexBuffers();

		GL_CHECK(glDisable(GL_DEPTH_TEST));

		mProjectionType = ProjectionType::Ortho2D;

		// Reset camera and model matrices
		m3dCameraMatrix = glm::mat4();
		m3dModelMatrix = glm::mat4();

		// Recalculate combined matrix
		m3dProjectionMatrix = glm::mat4();
		m3dModelCameraProjectionMatrix = m3dProjectionMatrix * m3dCameraMatrix * m3dModelMatrix;
	}

	/*
	 * Translate the model matrix in 2d.
	 *
	 */
	void RenderSystem::translateTransform2d(glm::vec2 const& vec)
	{
		translateTransform3d(glm::vec3(vec.x, vec.y, 0.0f));
	}

	/*
	 * Rotate the model matrix in 2d.
	 *
	 */
	void RenderSystem::rotateTransform2d(float angle)
	{
		rotateTransform3d(angle, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	/*
	 * Scale the model matrix in 2d.
	 * 
	 */
	void RenderSystem::scaleTransform2d(glm::vec2 const& vec)
	{
		scaleTransform3d(glm::vec3(vec.x, vec.y, 1.0f));
	}

	void RenderSystem::setAmbientColour(Colour const& colour)
	{
		const uint32_t offset{ 0 };
		auto fp = (float*)(&mLightsBuffer->getBufferData()[0]);
		fp += offset;

		// Ambient
		*fp++ = colour.red;
		*fp++ = colour.green;
		*fp++ = colour.blue;

		mLightsBuffer->updateData(offset * sizeof(float), 12);
	}

	void RenderSystem::setLight1Position(glm::vec3 const& pos)
	{
		const uint32_t offset{ 4 };
		auto fp = (float*)(&mLightsBuffer->getBufferData()[0]);
		fp += offset;

		// Light1 position
		*fp++ = pos.x;
		*fp++ = pos.y;
		*fp++ = pos.z;

		mLightsBuffer->updateData(offset * sizeof(float), 12);
	}

	void RenderSystem::setLight1Colour(Colour const& colour)
	{
		const uint32_t offset{ 8 };
		auto fp = (float*)(&mLightsBuffer->getBufferData()[0]);
		fp += offset;

		// Light1 position
		*fp++ = colour.red;
		*fp++ = colour.green;
		*fp++ = colour.blue;

		mLightsBuffer->updateData(offset * sizeof(float), 12);
	}

	void RenderSystem::setLight2Position(glm::vec3 const& pos)
	{
		const uint32_t offset{ 12 };
		auto fp = (float*)(&mLightsBuffer->getBufferData()[0]);
		fp += offset;

		// Light1 position
		*fp++ = pos.x;
		*fp++ = pos.y;
		*fp++ = pos.z;

		mLightsBuffer->updateData(offset * sizeof(float), 12);
	}

	void RenderSystem::setLight2Colour(Colour const& colour)
	{
		const uint32_t offset{ 16 };
		auto fp = (float*)(&mLightsBuffer->getBufferData()[0]);
		fp += offset;

		// Light2 position
		*fp++ = colour.red;
		*fp++ = colour.green;
		*fp++ = colour.blue;

		mLightsBuffer->updateData(offset * sizeof(float), 12);
	}

	void RenderSystem::setLightCount(size_t count)
	{
		const uint32_t offset{ 20 };

		auto fp = (int*)(&mLightsBuffer->getBufferData()[0]);
		fp += offset;

		// Count
		*fp = (int)count;

		mLightsBuffer->updateData(offset * sizeof(float), 4);
	}

	/*
	 * Clear the screen to the specified colour.
	 *
	 */
	void RenderSystem::clearScreen(Colour const& colour)
	{
		GL_CHECK(glClearColor(colour.red, colour.green, colour.blue, colour.alpha));

		glEnable(GL_SCISSOR_TEST);
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
		glDisable(GL_SCISSOR_TEST);
	}

	void RenderSystem::setGamma(float gamma)
	{
		mGamma = gamma;
	}

	float RenderSystem::getGamma() const
	{
		return mGamma;
	}

	/*
	 * Render a model.
	 *
	 */
	ModelInstance* RenderSystem::renderModelBatched(Model const& model, bool alphaBlend)
	{
		return renderModelBatched(model, alphaBlend, glm::vec3(0.0f, 0.0f, 0.0f), m3dModelMatrix, m3dModelCameraProjectionMatrix);
	}

	ModelInstance* RenderSystem::renderModelBatched(Model const& model, bool alphaBlend, CameraPtr camera)
	{
		return renderModelBatched(model, alphaBlend, glm::vec3(0.0f, 0.0f, 0.0f), m3dModelMatrix, camera->getProjectionTransform() * camera->getViewTransform());
	}

	ModelInstance* RenderSystem::renderModelBatched(Model const& model, bool alphaBlend, glm::vec3 const& viewPos)
	{
		return renderModelBatched(model, alphaBlend, viewPos, m3dModelMatrix, m3dModelCameraProjectionMatrix);
	}

	ModelInstance* RenderSystem::renderModelBatched(Model const& model, glm::mat4 const& transform, CameraPtr camera)
	{
		return renderModelBatched(model, true, camera->getPosition(), transform, camera->getProjectionTransform() * camera->getViewTransform() * transform);
	}

	ModelInstance* RenderSystem::renderModelBatched(Model const& model, bool alphaBlend, glm::vec3 const& viewPos, glm::mat4 const& transform, glm::mat4 const& mcp)
	{
		auto modelInstance = mModelInstances->acquireObject();
		
		modelInstance->setup(
			model,
			viewPos,
			transform,
			mcp,
			glm::transpose(glm::inverse(glm::mat3(transform))),
			glm::vec2(mRenderTarget->getWidth() / 2.0f, mRenderTarget->getHeight() / 2.0f),
			getGamma(),
			mMeshInstances);

		auto& instances = modelInstance->getMeshInstances();
		for (auto& instance : instances)
		{
			instance->blend(alphaBlend);
		}

		return modelInstance;
	}

	/*
	 * Render a model.
	 *
	 */
	void RenderSystem::renderModelImmediate(Model const& model, bool alphaBlend, glm::vec3 const& viewPos, shared_ptr<ModelRenderParams> params)
	{
		renderModelBatched(model, alphaBlend, viewPos)->setParams(params);
		flushVertexBuffers();
	}

	void RenderSystem::renderModelImmediate(Model const& model, bool alphaBlend, shared_ptr<ModelRenderParams> params)
	{
		renderModelBatched(model, alphaBlend)->setParams(params);
		flushVertexBuffers();
	}

	void RenderSystem::renderModelImmediate(Model const& model, bool alphaBlend, shared_ptr<ModelRenderParams> params, CameraPtr camera)
	{
		renderModelBatched(model, alphaBlend, camera)->setParams(params);
		flushVertexBuffers();
	}

	/*
	 * Set up the model used for rendering text
	 *
	 */
	int RenderSystem::buildTextVertexBuffer(VertexBuffer* buffer, string const& text, int& offset, int x, int y)
	{
		char const* textPtr = text.c_str();
		int numChars = (int)strlen(textPtr);

		int vertexStride = (int)buffer->getVertexStride() / sizeof(float);

		vector<int8_t>& bufferData = buffer->getBufferData();

		float xpos = (float)x + 8; // 8 to offset default kerning
		float ypos = (float)y;

		if (mCaps.pointSizeRange[1] >= 16)
		{
			auto glyphOffset = (uint32_t)(offset + numChars);
			if (glyphOffset > MaxTextGlyphs)
			{
				return 0;
			}

			float* bufferPtr = (float*)&(bufferData[offset * vertexStride * sizeof(float)]);

			for (int i = 0; i < numChars; ++i)
			{
				Font::Glyph const& glyph = mInternalFont->getGlyph(textPtr[i]);
				xpos += glyph.kern / 2;

				// Set data
				bufferPtr[0] = xpos;
				bufferPtr[1] = ypos + glyph.raise;
				bufferPtr[2] = glyph.u0_;
				bufferPtr[3] = glyph.v0_;
				bufferPtr[4] = glyph.u1_;
				bufferPtr[5] = glyph.v1_;

				bufferPtr += vertexStride;

				xpos += (glyph.width + glyph.kern / 2);
			}

			offset += numChars;
			return numChars;
		}
		else
		{
			auto glyphOffset = (uint32_t)(offset + numChars * 6);
			if (glyphOffset > MaxTextGlyphs)
			{
				return 0;
			}

			float* bufferPtr = (float*)&(bufferData[offset * vertexStride * sizeof(float)]);

			for (int i = 0; i < numChars; ++i)
			{
				Font::Glyph const& glyph = mInternalFont->getGlyph(textPtr[i]);
				xpos += glyph.kern / 2;

				// Set data
				bufferPtr[0] = xpos;
				bufferPtr[1] = ypos + glyph.raise;
				bufferPtr[2] = glyph.u0_;
				bufferPtr[3] = glyph.v0_;

				bufferPtr[4] = xpos;
				bufferPtr[5] = ypos + glyph.height + glyph.raise;
				bufferPtr[6] = glyph.u0_;
				bufferPtr[7] = glyph.v1_;

				bufferPtr[8] = xpos + glyph.width;
				bufferPtr[9] = ypos + glyph.height + glyph.raise;
				bufferPtr[10] = glyph.u1_;
				bufferPtr[11] = glyph.v1_;

				bufferPtr[12] = xpos + glyph.width;
				bufferPtr[13] = ypos + glyph.height + glyph.raise;
				bufferPtr[14] = glyph.u1_;
				bufferPtr[15] = glyph.v1_;

				bufferPtr[16] = xpos + glyph.width;
				bufferPtr[17] = ypos + glyph.raise;
				bufferPtr[18] = glyph.u1_;
				bufferPtr[19] = glyph.v0_;

				bufferPtr[20] = xpos;
				bufferPtr[21] = ypos + glyph.raise;
				bufferPtr[22] = glyph.u0_;
				bufferPtr[23] = glyph.v0_;

				bufferPtr += vertexStride * 6;

				xpos += (glyph.width + glyph.kern / 2);
			}

			offset += numChars * 6;
			return numChars * 2;
		}
	}

	/*
	 * Set up the model used for rendering text
	 *
	 */
	int RenderSystem::buildColouredTextVertexBuffer(VertexBuffer* buffer, string const& text, int& offset, int x, int y)
	{
		char const* textPtr = text.c_str();
		int numChars = (int)strlen(textPtr);

		int vertexStride = (int)buffer->getVertexStride() / sizeof(float);

		vector<int8_t>& bufferData = buffer->getBufferData();

		float xpos = (float)x + 8; // 8 to offset default kerning
		float ypos = (float)y;

		Colour colour = Colour::White;

		if (mCaps.pointSizeRange[1] >= 16)
		{
			auto glyphOffset = (uint32_t)(offset + numChars);
			if (glyphOffset > MaxTextGlyphs)
			{
				return 0;
			}

			float* bufferPtr = (float*)&(bufferData[offset * vertexStride * sizeof(float)]);

			int i = 0, numSpecial = 0;
			while (i < numChars)
			{
				// Format for colour is [#RRGGBBAA]
				// Colours are hex, and should be converted to uint8_t
				// Then change text spec from float to uint8_t
				// Pack the value into a uint32_t and reinterpret_cast to float
				if (i < (numChars - 8) && textPtr[i] == '[' && 
					textPtr[i + 1] == '#' && textPtr[i + 10] == ']')
				{
					uint8_t tcolour[4] = { 0, 0, 0, 0 };
					for (int j = 0; j < 8; ++j)
					{
						if (textPtr[i + j + 2] >= '0' && textPtr[i + j + 2] <= '9')
						{
							tcolour[j / 2] += (textPtr[i + j + 2] - '0') << (4 * (1 - (j % 2)));
						}
						else if (textPtr[i + j + 2] >= 'A' && textPtr[i + j + 2] <= 'F')
						{
							tcolour[j / 2] += (10 + (textPtr[i + j + 2] - 'A')) << (4 * (1 - (j % 2)));
						}
						else if (textPtr[i + j + 2] >= 'a' && textPtr[i + j + 2] <= 'f')
						{
							tcolour[j / 2] += (10 + (textPtr[i + j + 2] - 'a')) << (4 * (1 - (j % 2)));
						}
						else
						{
							goto no_ctrl;
						}
					}

					numSpecial += 11;
					i += 11;
					colour.red = tcolour[0] / 255.0f;
					colour.green = tcolour[1] / 255.0f;
					colour.blue = tcolour[2] / 255.0f;
					colour.alpha = tcolour[3] / 255.0f;
				}
				
			no_ctrl:;

				Font::Glyph const& glyph = mInternalFont->getGlyph(textPtr[i]);
				xpos += glyph.kern / 2;

				// Set data
				bufferPtr[0] = xpos;
				bufferPtr[1] = ypos + glyph.raise;
				bufferPtr[2] = glyph.u0_;
				bufferPtr[3] = glyph.v0_;
				bufferPtr[4] = glyph.u1_;
				bufferPtr[5] = glyph.v1_;
				bufferPtr[6] = colour.red;
				bufferPtr[7] = colour.green;
				bufferPtr[8] = colour.blue;
				bufferPtr[9] = colour.alpha;

				bufferPtr += vertexStride;

				xpos += (glyph.width + glyph.kern / 2);
				i++;
			}

			offset += (numChars - numSpecial);
			return (numChars - numSpecial);
		}
		else
		{
			auto glyphOffset = (uint32_t)(offset + numChars * 6);
			if (glyphOffset > MaxTextGlyphs)
			{
				return 0;
			}

			float* bufferPtr = (float*)&(bufferData[offset * vertexStride * sizeof(float)]);

			for (int i = 0; i < numChars; ++i)
			{
				Font::Glyph const& glyph = mInternalFont->getGlyph(textPtr[i]);
				xpos += glyph.kern / 2;

				// Set data
				bufferPtr[0] = xpos;
				bufferPtr[1] = ypos + glyph.raise;
				bufferPtr[2] = glyph.u0_;
				bufferPtr[3] = glyph.v0_;
				bufferPtr[4] = colour.red;
				bufferPtr[5] = colour.green;
				bufferPtr[6] = colour.blue;
				bufferPtr[7] = colour.alpha;

				bufferPtr[8] = xpos;
				bufferPtr[9] = ypos + glyph.height + glyph.raise;
				bufferPtr[10] = glyph.u0_;
				bufferPtr[11] = glyph.v1_;
				bufferPtr[12] = colour.red;
				bufferPtr[13] = colour.green;
				bufferPtr[14] = colour.blue;
				bufferPtr[15] = colour.alpha;

				bufferPtr[16] = xpos + glyph.width;
				bufferPtr[17] = ypos + glyph.height + glyph.raise;
				bufferPtr[18] = glyph.u1_;
				bufferPtr[19] = glyph.v1_;
				bufferPtr[20] = colour.red;
				bufferPtr[21] = colour.green;
				bufferPtr[22] = colour.blue;
				bufferPtr[23] = colour.alpha;

				bufferPtr[24] = xpos + glyph.width;
				bufferPtr[25] = ypos + glyph.height + glyph.raise;
				bufferPtr[26] = glyph.u1_;
				bufferPtr[27] = glyph.v1_;
				bufferPtr[28] = colour.red;
				bufferPtr[29] = colour.green;
				bufferPtr[30] = colour.blue;
				bufferPtr[31] = colour.alpha;

				bufferPtr[32] = xpos + glyph.width;
				bufferPtr[33] = ypos + glyph.raise;
				bufferPtr[34] = glyph.u1_;
				bufferPtr[35] = glyph.v0_;
				bufferPtr[36] = colour.red;
				bufferPtr[37] = colour.green;
				bufferPtr[38] = colour.blue;
				bufferPtr[39] = colour.alpha;

				bufferPtr[40] = xpos;
				bufferPtr[41] = ypos + glyph.raise;
				bufferPtr[42] = glyph.u0_;
				bufferPtr[43] = glyph.v0_;
				bufferPtr[44] = colour.red;
				bufferPtr[45] = colour.green;
				bufferPtr[46] = colour.blue;
				bufferPtr[47] = colour.alpha;

				bufferPtr += vertexStride * 6;

				xpos += (glyph.width + glyph.kern / 2);
			}

			offset += numChars * 6;
			return numChars * 2;
		}
	}

	void RenderSystem::createLightsData()
	{
		if (mLightsBuffer)
		{
			delete mLightsBuffer;
		}

		// Set up lights uniform buffer
		const size_t uniformSize{ 96 };
		shared_ptr<const int8_t> uniformData(new int8_t[uniformSize], [](int8_t *p) { delete[] p; });

		auto fp = (float*)uniformData.get();

		// Ambient
		*fp++ = 0.0f;
		*fp++ = 0.0f;
		*fp++ = 0.0f;
		fp++;

		// Light1 position
		*fp++ = 0.0f;
		*fp++ = 0.0f;
		*fp++ = 0.0f;
		fp++;

		// Light1 colour
		*fp++ = 1.0f;
		*fp++ = 1.0f;
		*fp++ = 1.0f;
		fp++;

		// Light2 position
		*fp++ = 0.0f;
		*fp++ = 0.0f;
		*fp++ = 0.0f;
		fp++;

		// Light2 colour
		*fp++ = 1.0f;
		*fp++ = 1.0f;
		*fp++ = 1.0f;
		fp++;

		// Count
		*(int32_t*)(fp) = 0;

		mLightsBuffer = new UniformBuffer(this, uniformData, uniformSize, 0);
		mLightsBuffer->load();
	}

	void RenderSystem::destroyLightsData()
	{
		if (mLightsBuffer)
		{
			mLightsBuffer->unload();
			delete mLightsBuffer;
			mLightsBuffer = nullptr;
		}
	}

	void RenderSystem::createPbrLightsData()
	{
		destroyPbrLightsData();

		// std140 layout: vec4 ambientAndCount, followed by MaxPbrLights entries
		// of vec4 colourIntensity, vec4 positionRange, vec4 directionType.
		const size_t uniformSize = 16 + MaxPbrLights * 48;
		shared_ptr<const int8_t> uniformData(new int8_t[uniformSize](), [](int8_t* p) { delete[] p; });
		auto fp = reinterpret_cast<float*>(const_cast<int8_t*>(uniformData.get()));
		fp[0] = 0.03f;
		fp[1] = 0.03f;
		fp[2] = 0.03f;
		fp[3] = 0.0f;

		mPbrLightsBuffer = new UniformBuffer(this, uniformData, uniformSize, 1);
		mPbrLightsBuffer->load();
	}

	void RenderSystem::destroyPbrLightsData()
	{
		if (mPbrLightsBuffer)
		{
			mPbrLightsBuffer->unload();
			delete mPbrLightsBuffer;
			mPbrLightsBuffer = nullptr;
		}
	}

	void RenderSystem::setPbrAmbientColour(Colour const& colour)
	{
		auto fp = reinterpret_cast<float*>(mPbrLightsBuffer->getBufferData().data());
		fp[0] = colour.red;
		fp[1] = colour.green;
		fp[2] = colour.blue;
		mPbrLightsBuffer->updateData(0, 12);
	}

	void RenderSystem::setPbrLights(vector<PbrLight> const& lights)
	{
		if (lights.size() > MaxPbrLights)
		{
			THROW_MPP("Too many PBR lights for the PBR light uniform buffer.", __LINE__, __FILE__, __func__);
		}

		auto& data = mPbrLightsBuffer->getBufferData();
		fill(data.begin() + 16, data.end(), 0);
		auto fp = reinterpret_cast<float*>(data.data());
		fp[3] = (float)lights.size();
		for (size_t i = 0; i < lights.size(); ++i)
		{
			auto const& light = lights[i];
			fp += 4 + i * 12;
			fp[0] = light.colour.r;
			fp[1] = light.colour.g;
			fp[2] = light.colour.b;
			fp[3] = light.intensity;
			fp[4] = light.position.x;
			fp[5] = light.position.y;
			fp[6] = light.position.z;
			fp[7] = light.range;
			fp[8] = light.direction.x;
			fp[9] = light.direction.y;
			fp[10] = light.direction.z;
			fp[11] = light.type == PbrLightType::Point ? 1.0f : 0.0f;
			fp -= 4 + i * 12;
		}
		mPbrLightsBuffer->mapBufferData();
	}

	void RenderSystem::destroyShadowDomains()
	{
		mActiveShadowDepthTarget.reset();
		mShadowDisabledFrameBuffer.reset();
		mShadowDomains.clear();
	}

	void RenderSystem::createShadowDisabledFrameBuffer()
	{
		if (mShadowDisabledFrameBuffer)
		{
			return;
		}
		ShadowFrameData frame;
		shared_ptr<const int8_t> frameBytes(new int8_t[sizeof(frame)](), [](int8_t* p) { delete[] p; });
		memcpy(const_cast<int8_t*>(frameBytes.get()), &frame, sizeof(frame));
		mShadowDisabledFrameBuffer = make_shared<UniformBuffer>(this, frameBytes, sizeof(frame), 2);
		mShadowDisabledFrameBuffer->load();
	}

	void RenderSystem::createShadowDomainResources(string const& name, ShadowDomainState& domain)
	{
		if (!domain.options.enabled || domain.depthTarget)
		{
			return;
		}
		if (!mResourceMgr)
		{
			THROW_MPP("Cannot create a shadow domain before core resources are available.", __LINE__, __FILE__, __func__);
		}

		RenderTextureOptions targetOptions;
		targetOptions.numAttachments = 0;
		targetOptions.depthAttachment = RenderTextureDepthAttachment::DepthTexture;
		targetOptions.depthParams.params.minFilter = GL_LINEAR;
		targetOptions.depthParams.params.magFilter = GL_LINEAR;
		targetOptions.depthParams.params.wrap = GL_CLAMP_TO_BORDER;
		targetOptions.depthParams.compareRefToTexture = true;
		domain.depthTarget = createRenderTexture(
			"ShadowDomain." + name + ".Depth",
			domain.options.resolution,
			domain.options.resolution,
			targetOptions);

		// std140: mat4 (64 bytes), then two vec4 values. Keep this independent
		// of PBR lighting so legacy/custom shaders can use the same frame data.
		ShadowFrameData frame;
		const float texelSize = 1.0f / (float)domain.options.resolution;
		frame.mapTexelSizeAndRadius = glm::vec4(texelSize, texelSize, domain.options.filterRadiusTexels,
			domain.options.filterMode == ShadowFilterMode::Pcf3x3 ? 1.0f : 0.0f);
		frame.biasAndEnabled = glm::vec4(domain.options.constantBias, domain.options.normalBias, 1.0f, 0.0f);

		shared_ptr<const int8_t> frameBytes(new int8_t[sizeof(frame)](), [](int8_t* p) { delete[] p; });
		memcpy(const_cast<int8_t*>(frameBytes.get()), &frame, sizeof(frame));
		domain.frameBuffer = make_shared<UniformBuffer>(this, frameBytes, sizeof(frame), 2);
		domain.frameBuffer->load();
	}

	void RenderSystem::configureShadowDomain(string const& name, ShadowOptions const& options)
	{
		if (name.empty())
		{
			THROW_MPP("Shadow domain name cannot be empty.", __LINE__, __FILE__, __func__);
		}
		if (options.enabled)
		{
			if (options.resolution == 0 || options.orthoHalfWidth <= 0.0f || options.nearPlane < 0.0f ||
				options.farPlane <= options.nearPlane || options.constantBias < 0.0f || options.normalBias < 0.0f ||
				options.filterRadiusTexels < 0.0f || !isfinite(options.orthoHalfWidth) ||
				!isfinite(options.nearPlane) || !isfinite(options.farPlane) ||
				!isfinite(options.constantBias) || !isfinite(options.normalBias) || !isfinite(options.filterRadiusTexels) ||
				!isfinite(options.light.direction.x) || !isfinite(options.light.direction.y) || !isfinite(options.light.direction.z) ||
				glm::dot(options.light.direction, options.light.direction) < 0.000001f)
			{
				THROW_MPP("Invalid shadow domain options.", __LINE__, __FILE__, __func__);
			}
		}

		auto& domain = mShadowDomains[name];
		domain.depthTarget.reset();
		domain.frameBuffer.reset();
		domain.options = options;
	}

	bool RenderSystem::hasShadowDomain(string const& name) const
	{
		return mShadowDomains.find(name) != mShadowDomains.end();
	}

	ShadowOptions const& RenderSystem::getShadowDomainOptions(string const& name) const
	{
		auto it = mShadowDomains.find(name);
		if (it == mShadowDomains.end())
		{
			THROW_MPP("Unknown shadow domain.", __LINE__, __FILE__, __func__);
		}
		return it->second.options;
	}

	RenderTargetPtr RenderSystem::getShadowDomainDepthTarget(string const& name)
	{
		ensureShadowDomainResources(name);
		return mShadowDomains.at(name).depthTarget;
	}

	void RenderSystem::ensureShadowDomainResources(string const& name)
	{
		auto it = mShadowDomains.find(name);
		if (it == mShadowDomains.end())
		{
			THROW_MPP("Pipeline references an unknown shadow domain.", __LINE__, __FILE__, __func__);
		}
		createShadowDomainResources(name, it->second);
	}

	void RenderSystem::renderShadowDomain(string const& name, vector<SceneModel3dPtr> const& models)
	{
		ensureShadowDomainResources(name);
		auto& domain = mShadowDomains.at(name);
		if (!domain.options.enabled)
		{
			return;
		}

		auto depthTarget = domain.depthTarget;
		auto shadowProgram = static_cast<Program*>(mShadowDepthProgram.get());
		glm::vec3 direction = glm::normalize(domain.options.light.direction);
		glm::vec3 up = abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		float lightDistance = (domain.options.farPlane - domain.options.nearPlane) * 0.5f;
		glm::mat4 lightView = glm::lookAt(domain.options.light.focusPoint - direction * lightDistance, domain.options.light.focusPoint, up);
		float extent = domain.options.orthoHalfWidth;
		glm::mat4 lightProjection = glm::ortho(-extent, extent, -extent, extent, domain.options.nearPlane, domain.options.farPlane);
		glm::mat4 lightViewProjection = lightProjection * lightView;

		ShadowFrameData frame;
		frame.lightViewProjection = lightViewProjection;
		float texelSize = 1.0f / (float)domain.options.resolution;
		frame.mapTexelSizeAndRadius = glm::vec4(texelSize, texelSize, domain.options.filterRadiusTexels,
			domain.options.filterMode == ShadowFilterMode::Pcf3x3 ? 1.0f : 0.0f);
		frame.biasAndEnabled = glm::vec4(domain.options.constantBias, domain.options.normalBias, 1.0f, 0.0f);
		auto& frameBytes = domain.frameBuffer->getBufferData();
		memcpy(frameBytes.data(), &frame, sizeof(frame));
		domain.frameBuffer->mapBufferData();

		GLint previousViewport[4];
		GLint previousCullMode;
		GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
		GLboolean blendEnabled = glIsEnabled(GL_BLEND);
		GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
		GLboolean polygonOffsetEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
		GLboolean depthWriteEnabled;
		GLfloat previousPolygonOffsetFactor, previousPolygonOffsetUnits;
		GL_CHECK(glGetIntegerv(GL_VIEWPORT, previousViewport));
		GL_CHECK(glGetIntegerv(GL_CULL_FACE_MODE, &previousCullMode));
		GL_CHECK(glGetBooleanv(GL_DEPTH_WRITEMASK, &depthWriteEnabled));
		GL_CHECK(glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &previousPolygonOffsetFactor));
		GL_CHECK(glGetFloatv(GL_POLYGON_OFFSET_UNITS, &previousPolygonOffsetUnits));

		pushRenderTarget(depthTarget);
		GL_CHECK(glViewport(0, 0, (GLsizei)depthTarget->getWidth(), (GLsizei)depthTarget->getHeight()));
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glDepthMask(GL_TRUE));
		GL_CHECK(glDisable(GL_BLEND));
		GL_CHECK(glEnable(GL_CULL_FACE));
		GL_CHECK(glCullFace(GL_FRONT));
		GL_CHECK(glEnable(GL_POLYGON_OFFSET_FILL));
		GL_CHECK(glPolygonOffset(2.0f, 4.0f));
		GL_CHECK(glClearDepth(1.0));
		GL_CHECK(glClear(GL_DEPTH_BUFFER_BIT));

		setUsedProgram(mShadowDepthProgram);
		for (auto const& sceneModel : models)
		{
			auto params = sceneModel->getParams();
			auto const& meshParams = params->getMeshParams();
			auto defaultParams = meshParams.find("");
			auto model = static_cast<Model*>(sceneModel->getModel().get());
			glm::mat4 modelLightProjection = lightViewProjection * sceneModel->getTransform();

			for (int meshIndex = 0; meshIndex < model->getNumMeshes(); ++meshIndex)
			{
				auto mesh = model->getMesh(meshIndex);
				auto meshParamsIt = meshParams.find(mesh->getName());
				auto const* renderParams = meshParamsIt != meshParams.end() ? &meshParamsIt->second :
					(defaultParams != meshParams.end() ? &defaultParams->second : nullptr);
				if (renderParams && (renderParams->flags & ModelRenderParams::Flag_Visible) == 0)
				{
					continue;
				}

				auto material = static_cast<Material*>(mesh->getMaterial().get());
				if (material->isPbr() && material->getPbrSurface().alphaMode == MaterialSpecification::PbrAlphaMode::Blend)
				{
					continue; // Conventional blended materials do not cast in S2.
				}

				GL_CHECK(glUniformMatrix4fv(shadowProgram->getModelCameraProjectionMatrixId(), 1, GL_FALSE, glm::value_ptr(modelLightProjection)));
				mesh->bind(true);
				size_t instanceCount = renderParams ? renderParams->instanceCount : 1;
				if (renderParams && !renderParams->renderCommands.empty())
				{
					for (auto const& command : renderParams->renderCommands)
					{
						mesh->render(instanceCount, command.offset, command.count);
					}
				}
				else
				{
					mesh->render(instanceCount);
				}
				mesh->bind(false);
			}
		}

		popRenderTarget();
		GL_CHECK(glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]));
		if (depthTestEnabled) GL_CHECK(glEnable(GL_DEPTH_TEST)); else GL_CHECK(glDisable(GL_DEPTH_TEST));
		if (blendEnabled) GL_CHECK(glEnable(GL_BLEND)); else GL_CHECK(glDisable(GL_BLEND));
		if (cullEnabled) GL_CHECK(glEnable(GL_CULL_FACE)); else GL_CHECK(glDisable(GL_CULL_FACE));
		GL_CHECK(glCullFace(previousCullMode));
		if (polygonOffsetEnabled) GL_CHECK(glEnable(GL_POLYGON_OFFSET_FILL)); else GL_CHECK(glDisable(GL_POLYGON_OFFSET_FILL));
		GL_CHECK(glPolygonOffset(previousPolygonOffsetFactor, previousPolygonOffsetUnits));
		GL_CHECK(glDepthMask(depthWriteEnabled));
		GL_CHECK(glUseProgram(0));
		mActiveProgram.reset();
	}

	void RenderSystem::setActiveShadowDomain(string const& name)
	{
		mActiveShadowDepthTarget.reset();
		if (name.empty())
		{
			if (mShadowDisabledFrameBuffer)
			{
				mShadowDisabledFrameBuffer->activate();
			}
			return;
		}

		ensureShadowDomainResources(name);
		auto& domain = mShadowDomains.at(name);
		if (domain.options.enabled)
		{
			domain.frameBuffer->activate();
			mActiveShadowDepthTarget = domain.depthTarget;
		}
		else if (mShadowDisabledFrameBuffer)
		{
			mShadowDisabledFrameBuffer->activate();
		}
	}

	void RenderSystem::setActivePbrEnvironment(PbrEnvironmentPtr environment)
	{
		mActivePbrEnvironment = std::move(environment);
		if (!mActivePbrEnvironment)
		{
			return;
		}

		for (auto const& resource : {
			mActivePbrEnvironment->irradianceMap,
			mActivePbrEnvironment->prefilteredSpecularMap,
			mActivePbrEnvironment->brdfIntegrationLut,
			mActivePbrEnvironment->backgroundMap })
		{
			if (resource)
			{
				resource->load();
			}
		}
	}

	void RenderSystem::setActivePipelineSamplerOverrides(map<string, ResourcePtr> const& overrides)
	{
		mActivePipelineSamplerOverrides = overrides;
		for (auto const& [sampler, resource] : mActivePipelineSamplerOverrides)
		{
			if (resource)
			{
				resource->load();
			}
		}
	}

	/*
	 * Render a texture as a fullscreen quad
	 *
	 */
	void RenderSystem::renderFullscreenQuad(Texture* texture, BlendMode srcBlend, BlendMode dstBlend, shared_ptr<UniformCollection> uniforms)
	{
		flushVertexBuffers();

		// Set program
		auto p = static_cast<Program*>(mFullscreenProgram.get());

		setUsedProgram(mFullscreenProgram);
		mRenderInfo.programSwitches++;

		// Set uniforms
		if (uniforms)
		{
			uniforms->bindUniforms(mFullscreenProgram);
		}
		else
		{
			// Set defaults manually
			GL_CHECK(glUniform4f(p->getUniformId("DIFFUSE"), 1, 1, 1, 1));
		}

		GL_CHECK(glUniformMatrix4fv(p->getModelCameraProjectionMatrixId(), 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));
		GL_CHECK(glUniform2f(p->getHalfWindowSizeId(), mRenderTarget->getWidth() / 2.0f, mRenderTarget->getHeight() / 2.0f));

		int gammaId = p->getUniformId("GAMMA");

		if (gammaId >= 0)
		{
			GL_CHECK(glUniform1f(gammaId, mGamma));
		}

		// Set texture
		texture->bind(0, 0);
		mRenderInfo.textureSwitches++;

		// Set blend
		GL_CHECK(glEnable(GL_BLEND));
		GL_CHECK(glBlendFunc((int)srcBlend, (int)dstBlend));

		// Bind mesh
		auto quadMesh = ((Model*)mFullscreenQuad.get())->getMesh(0);
		quadMesh->bind(true);
		quadMesh->render(1);

		// Unbind
		quadMesh->bind(false);

		// Disable blend
		GL_CHECK(glDisable(GL_BLEND));

		mRenderInfo.batchCount++;
		mRenderInfo.fullscreenQuads++;
	}

	void RenderSystem::renderToneMappedFullscreenQuad(Texture* texture, float exposure, bool useAcesToneMap)
	{
		flushVertexBuffers();

		auto program = static_cast<Program*>(mToneMapProgram.get());
		setUsedProgram(mToneMapProgram);
		mRenderInfo.programSwitches++;

		GL_CHECK(glUniformMatrix4fv(program->getModelCameraProjectionMatrixId(), 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));
		GL_CHECK(glUniform2f(program->getHalfWindowSizeId(), mRenderTarget->getWidth() / 2.0f, mRenderTarget->getHeight() / 2.0f));
		GL_CHECK(glUniform1f(program->getUniformId("EXPOSURE"), exposure));
		GL_CHECK(glUniform1f(program->getUniformId("GAMMA"), mGamma));
		GL_CHECK(glUniform1i(program->getUniformId("TONE_MAP_OPERATOR"), useAcesToneMap ? 1 : 0));

		texture->bind(0, 0);
		mRenderInfo.textureSwitches++;

		GL_CHECK(glDisable(GL_BLEND));
		auto quadMesh = static_cast<Model*>(mFullscreenQuad.get())->getMesh(0);
		quadMesh->bind(true);
		quadMesh->render(1);
		quadMesh->bind(false);

		mRenderInfo.batchCount++;
		mRenderInfo.fullscreenQuads++;
	}

	/*
	 * Render a simple quad.
	 *
	 */
	void RenderSystem::renderQuad(int x, int y, int width, int height, Colour const& colour, bool alphaBlend, bool wireFrame)
	{
		renderQuad(x, y, width, height, colour, alphaBlend, wireFrame, mNoTexture);
	}

	/*
	 * Render a simple quad.
	 *
	 */
	void RenderSystem::renderQuad(int x, int y, int width, int height, Colour const& colour, bool alphaBlend, bool wireFrame, ResourcePtr texture)
	{
		flushVertexBuffers();

		// Set program
		auto p = static_cast<Program*>(mFullscreenProgram.get());

		setUsedProgram(mFullscreenProgram);
		mRenderInfo.programSwitches++;

		pushModelMatrix();
		translateTransform2d(glm::vec2(x, mRenderTarget->getHeight() - y));
		GL_CHECK(scaleTransform2d(glm::vec2(width / (float)mRenderTarget->getWidth(), height / (float)mRenderTarget->getHeight())));

		GL_CHECK(glUniformMatrix4fv(p->getModelCameraProjectionMatrixId(), 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));
		GL_CHECK(glUniform2f(p->getHalfWindowSizeId(), mRenderTarget->getWidth() / 2.0f, mRenderTarget->getHeight() / 2.0f));

		int gammaId = p->getUniformId("GAMMA");

		if (gammaId >= 0)
		{
			GL_CHECK(glUniform1f(gammaId, mGamma));
		}

		GL_CHECK(glUniform4f(p->getUniformId("DIFFUSE"), colour.red, colour.green, colour.blue, colour.alpha));

		// Set texture
		((Texture*)texture.get())->bind(0);
		mRenderInfo.textureSwitches++;

		// Bind mesh
		if (alphaBlend)
		{
			GL_CHECK(glEnable(GL_BLEND));
			GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
		}

		if (wireFrame)
		{
			GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
		}

		auto quadMesh = ((Model*)mFullscreenQuad.get())->getMesh(0);
		quadMesh->bind(true);
		quadMesh->render(1);

		// Unbind
		quadMesh->bind(false);

		if (alphaBlend)
		{
			GL_CHECK(glDisable(GL_BLEND));
		}

		if (wireFrame)
		{
			GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
		}

		popModelMatrix();

		mRenderInfo.batchCount++;
	}

	/*
	 * Render a text string as a series of quads.
	 *
	 */
	void RenderSystem::renderText(string const& text, int x, int y, Colour const& colour)
	{
		// Set text mesh for updating
		Model* textModel = (Model*)mTextMesh.get();
		Mesh* textMesh = textModel->getMesh(0);
		VertexBuffer* vertexBuffer = textMesh->getVertexBuffer(0);

		y = (int)(mWindowHeight - y - 16);
		
		int offset = 0;
		int count = buildTextVertexBuffer(vertexBuffer, text, offset, x, y);
		vertexBuffer->mapBufferData(count);
		
		mTextUniforms->updateUniform("COLOUR", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
		mTextParams->setModelPrimitiveCount(count);

		renderModelImmediate(static_cast<Model const&>(*mTextMesh.get()), true, mTextParams);
	}
	
	/*
	 * Render lines of text.
	 *
	 */
	void RenderSystem::renderText(vector<string> const& text, int x, int y, Colour const& colour)
	{
		// Set text mesh for updating
		Model* textModel = (Model*)mTextMesh.get();
		Mesh* textMesh = textModel->getMesh(0);
		VertexBuffer* vertexBuffer = textMesh->getVertexBuffer(0);

		y = (int)(mWindowHeight - y - 16); 
		int count = 0, offset = 0;
		for (uint32_t i = 0; i < text.size(); ++i)
		{
			count += buildTextVertexBuffer(vertexBuffer, text[i], offset, x, y - i * 16);
		}

		vertexBuffer->mapBufferData(count);

		mTextUniforms->updateUniform("COLOUR", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
		mTextParams->setModelPrimitiveCount(count);

		renderModelImmediate(static_cast<Model const&>(*mTextMesh.get()), true, mTextParams);
	}

	/*
	 * Render coloured text using tags.
	 *
	 */
	void RenderSystem::renderTextFormatted(string const& text, int x, int y)
	{
		Model* textModel = (Model*)mColouredTextMesh.get();
		Mesh* textMesh = textModel->getMesh(0);
		VertexBuffer* vertexBuffer = textMesh->getVertexBuffer(0);

		y = (int)(mWindowHeight - y - 16);

		int offset = 0;
		int count = buildColouredTextVertexBuffer(vertexBuffer, text, offset, x, y);
		vertexBuffer->mapBufferData(count);

		mTextUniforms->updateUniform("COLOUR", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		mTextParams->setModelPrimitiveCount(count);

		renderModelImmediate(static_cast<Model const&>(*mColouredTextMesh.get()), true, mTextParams);
	}
	
	void RenderSystem::renderTextFormatted(vector<string> const& text, int x, int y)
	{
		Model* textModel = (Model*)mColouredTextMesh.get();
		Mesh* textMesh = textModel->getMesh(0);
		VertexBuffer* vertexBuffer = textMesh->getVertexBuffer(0);

		y = (int)(mWindowHeight - y - 16);
		int count = 0, offset = 0;
		for (uint32_t i = 0; i < text.size(); ++i)
		{
			count += buildColouredTextVertexBuffer(vertexBuffer, text[i], offset, x, y - i * 16);
		}

		vertexBuffer->mapBufferData(count);

		mTextUniforms->updateUniform("COLOUR", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		mTextParams->setModelPrimitiveCount(count);

		renderModelImmediate(static_cast<Model const&>(*mColouredTextMesh.get()), true, mTextParams);
	}

	void RenderSystem::renderBufferImmediate(int8_t const* vertexData, uint32_t vertexStride, uint32_t numVertices, int8_t* const indexData, uint32_t indexWidth, uint32_t numIndices, vector<VertexBufferRenderCommand> const& commands)
	{
		flushVertexBuffers();

		size_t indexBytes = indexWidth >> 3;
		bool useIndices = indexData != nullptr && (indexBytes == 2 || indexBytes == 4);

		GL_CHECK(glEnable(GL_SCISSOR_TEST));
		GL_CHECK(glEnable(GL_BLEND));
		GL_CHECK(glBlendEquation(GL_FUNC_ADD));
		GL_CHECK(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));

		// Create VAO.  The VAO stores the vertex attributes and the VBO and IBO
		GLuint vao{ 0 };
		GL_CHECK(glGenVertexArrays(1, &vao));

		GL_CHECK(glBindVertexArray(vao));
		
		GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, mInternalVBO));

		if (useIndices)
		{
			GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mInternalIBO));
		}

		// Enable vertex attributes
		size_t posOffset{ 0 }, texOffset{ 8 }, colOffset{ 16 };

		GL_CHECK(glEnableVertexAttribArray(0));
		GL_CHECK(glEnableVertexAttribArray(1));
		GL_CHECK(glEnableVertexAttribArray(2));
		GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (GLvoid const*)posOffset));
		GL_CHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, (GLsizei)vertexStride, (GLvoid const*)texOffset));
		GL_CHECK(glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, (GLsizei)vertexStride, (GLvoid const*)colOffset));

		// Upload vertex and index data
		size_t vertexDataSize = vertexStride * numVertices;
		GL_CHECK(glBufferData(GL_ARRAY_BUFFER, (GLsizei)vertexDataSize, (GLvoid const*)vertexData, GL_STREAM_DRAW));
		
		if (useIndices)
		{
			size_t indexDataSize = indexBytes * numIndices;
			GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizei)indexDataSize, (GLvoid const*)indexData, GL_STREAM_DRAW));
		}

		// Render
		for (auto const& cmd : commands)
		{
			// Set program, uniforms, textures
			auto materialRes = cmd.material ? cmd.material : mInternalMaterial;
			auto mat = static_cast<Material*>(materialRes.get());

			setUsedProgram(mat->getProgram());
			mat->setUniforms();

			// Uniforms
			auto p = static_cast<Program*>(mat->getProgram().get());
			
			auto hwsId = p->getHalfWindowSizeId();
			auto mcpId = p->getModelCameraProjectionMatrixId();

			if (hwsId >= 0)
			{
				glm::vec2 halfWindowSize(mRenderTarget->getWidth() / 2.0f, mRenderTarget->getHeight() / 2.0f);
				GL_CHECK(glUniform2fv(hwsId, 1, glm::value_ptr(halfWindowSize)));
			}

			if (mcpId >= 0)
			{
				glm::mat4 mcp(
					cmd.scale[0], 0.0f, 0.0f, 0.0f,
					0.0f, cmd.scale[1], 0.0f, 0.0f,
					0.0f, 0.0f, cmd.scale[2], 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f
				);
				
				GL_CHECK(glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(mcp)));
			}

			int gammaId = p->getUniformId("GAMMA");

			if (gammaId >= 0)
			{
				GL_CHECK(glUniform1f(gammaId, mGamma));
			}

			// Textures
			for (int i = 0; i < mat->getNumTextures(); ++i)
			{
				auto textureRes = (size_t)i < cmd.textures.size() && cmd.textures[i]
					? cmd.textures[i]
					: mat->getTexture(i);
				static_cast<Texture*>(textureRes.get())->bind((uint32_t)i);
			}

			if (cmd.clipSize[0] > 0 && cmd.clipSize[1] > 0)
			{
				glScissor(cmd.clipMin[0], cmd.clipMin[1], cmd.clipSize[0], cmd.clipSize[1]);
			}

			// Draw
			if (useIndices)
			{
				GLenum indexType = indexBytes == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

				auto offset = (void*)(intptr_t)(cmd.offset * indexBytes);
				size_t count = cmd.count != ~0u ? (size_t)cmd.count : (numIndices - cmd.offset);

				GL_CHECK(glDrawElements(GL_TRIANGLES, (GLsizei)count, indexType, offset));
			}
			else
			{
				uint32_t offset = cmd.offset * 3;
				size_t count = cmd.count != ~0u ? (size_t)(cmd.count * 3) : (numVertices - cmd.offset * 3);

				GL_CHECK(glDrawArrays(GL_TRIANGLES, offset, (GLsizei)count));
			}
		}

		// Destroy VAO
		GL_CHECK(glDeleteVertexArrays(1, &vao));

		GL_CHECK(glDisable(GL_SCISSOR_TEST));
		GL_CHECK(glDisable(GL_BLEND));
	}

	void RenderSystem::setDebugPreMessages(vector<string> const& messages)
	{
		mDebugPreMessages = messages;
	}

	void RenderSystem::setDebugPostMessages(vector<string> const& messages)
	{
		mDebugPostMessages = messages;
	}

	/*
	 * Render debugging panel.
	 *
	 */
	void RenderSystem::renderDebugPanel()
	{
		setProjection2dOrthographic();

		vector<string> lines;

		copy(mDebugPreMessages.begin(), mDebugPreMessages.end(), back_inserter(lines));

#ifdef MPP_PROFILE_BUILD
		map<string, uint64_t> profileResults = mProfiler->getSamples();

		TimeSample timeSample;
		timeSample.frameTime = -1;
		timeSample.driverWaitsGPU = -1;
		timeSample.driverWaitsKernel = -1;
		timeSample.driverWaitsLock = -1;
		timeSample.driverWaitsRender = -1;
		timeSample.driverWaitsSwap = -1;

		for (auto it = profileResults.begin(); it != profileResults.end(); ++it)
		{
			string profile = it->first;
			uint64_t result = it->second;

			string msg, unit;

			if (profile == "OGL frame time")
			{
				timeSample.frameTime = (int)result;
			}
			else if (profile == "OGL driver waits for GPU")
			{
				timeSample.driverWaitsGPU = (int)result;
			}
			else if (profile == "OGL driver waits for GPU")
			{
				timeSample.driverWaitsKernel = (int)result;
			}
			else if (profile == "OGL driver waits for kernel")
			{
				timeSample.driverWaitsKernel = (int)result;
			}
			else if (profile == "OGL driver waits for lock")
			{
				timeSample.driverWaitsLock = (int)result;
			}
			else if (profile == "OGL driver waits for render")
			{
				timeSample.driverWaitsRender = (int)result;
			}
			else if (profile == "OGL driver waits for swap")
			{
				timeSample.driverWaitsSwap = (int)result;
			}

			if (profile == "OGL frame time" ||
				profile == "OGL driver time waiting" ||
				profile == "OGL driver waits for GPU" ||
				profile == "OGL driver waits for kernel" ||
				profile == "OGL driver waits for lock" ||
				profile == "OGL driver waits for render" ||
				profile == "OGL driver waits for swap")
			{
				switch (mTimeUnit)
				{
				case TimeUnit::Nanoseconds:
					unit = "ns";
					result *= 1000000;
					break;
				case TimeUnit::Microseconds:
					unit = "us";
					result *= 1000;
					break;
				case TimeUnit::Milliseconds:
					unit = "ms";
					result *= 1;
					break;
				case TimeUnit::Seconds:
					unit = "s";
					result /= 1000;
					break;
				}

				msg = STR_FORMAT("{}: {} {}", profile, result, unit);
			}
			else if (profile == "OGL memory allocated" ||
				profile == "OGL memory allocated (textures)" ||
				profile == "OGL memory allocated (vertex)")
			{
				switch (mSizeUnit)
				{
				case SizeUnit::Gigabytes:
					unit = "Gb";
					result /= 1000000000;
					break;
				case SizeUnit::Megabytes:
					unit = "Mb";
					result /= 1000000;
					break;
				case SizeUnit::Kilobytes:
					unit = "Kb";
					result /= 1000;
					break;
				case SizeUnit::Bytes:
					unit = "b";
					result *= 1;
					break;
				}

				msg = STR_FORMAT("{}: {} {}", profile, result, unit);
			}
			else if (profile == "Total GPU memory" ||
				profile == "Total available GPU memory" ||
				profile == "Current available GPU memory")
			{
				msg = STR_FORMAT("{}: {} Kb", profile, result);
			}
			else
			{
				msg = STR_FORMAT("{}: {}", profile, result);
			}

			lines.push_back(msg);
		}
#else
		lines.push_back(STR_FORMAT("Batches: {}", mRenderInfo.batchCount));
		lines.push_back(STR_FORMAT("Primitives: {}", mRenderInfo.primitivesRendered));
#endif

		lines.push_back(STR_FORMAT("Program switches: {}", mRenderInfo.programSwitches));
		lines.push_back(STR_FORMAT("Texture switches: {}", mRenderInfo.textureSwitches));
		lines.push_back(STR_FORMAT("Screen quads: {}", mRenderInfo.fullscreenQuads));

		copy(mDebugPostMessages.begin(), mDebugPostMessages.end(), back_inserter(lines));

		// Get resource info
		uint32_t numTotalResources, numDeclaredResources, numCreatedResources, numLoadedResources;
		mResourceMgr->getResourceCounts(numTotalResources, numDeclaredResources, numCreatedResources, numLoadedResources);

		lines.push_back(STR_FORMAT("Resources : {} ([#FF0000FF]{}[#FFFFFFFF] / [#FFFF00FF]{}[#FFFFFFFF] / [#00FF00FF]{}[#FFFFFFFF])", 
			numTotalResources, 
			numDeclaredResources, 
			numCreatedResources, 
			numLoadedResources));

		// Calculate padding and placement
		size_t width = 0;
		for (auto const& line: lines)
		{
			auto thisWidth = line.length();

			// Factor out colour formatting
			string::size_type pos{ 0 };
			while ((pos = line.find("[#", pos)) != string::npos)
			{
				thisWidth -= 11;
				pos += 2;
			}

			width = max(width, thisWidth);
		}

		// Background, in case the screen in that location is the same colour as the text
		int w = (int)width * 8, h = (int)(lines.size() + 1) * 16 - 8;
		int x = 0, y = (GLsizei)mWindowHeight - h;
		
		renderQuad(x, (GLsizei)mWindowHeight, w, h, Colour(0.5f, 0.625f, 0.87f, 0.85f), true, false);

		// Render batch information
		renderTextFormatted(lines, x, y);

#ifdef MPP_PROFILE_BUILD

		x = x + w + 10;
		w = mSamplesToRecord;
		h = 32 * 4;

		renderQuad(x, mWindowHeight, w, h, Colour(0.5f, 0.625f, 0.87f, 0.85f), true, false);

		// Render graph
		while (mProfileTimeSamples.size() >= mSamplesToRecord)
		{
			mProfileTimeSamples.pop_front();
		}
		
		mProfileTimeSamples.push_back(timeSample);
		
		mProfileLines->startUpdate(mSamplesToRecord * 5 + 2);
		float* posDataDst = (float*)mProfileLines->getPositionData();

		int xp = x, yp;
		for (uint32_t i = 0; i < mProfileTimeSamples.size(); ++i)
		{
			yp = 0;
			auto const& sample = mProfileTimeSamples[i];

			// Driver waiting for GPU
			*posDataDst++ = xp + i;
			*posDataDst++ = yp;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;

			*posDataDst++ = xp + i;
			*posDataDst++ = yp + sample.driverWaitsGPU * 4;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;

			yp += sample.driverWaitsGPU * 4;

			// Driver waiting for kernel
			*posDataDst++ = xp + i;
			*posDataDst++ = yp;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;

			*posDataDst++ = xp + i;
			*posDataDst++ = yp + sample.driverWaitsKernel * 4;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;

			yp += sample.driverWaitsKernel * 4;

			// Driver waiting for lock
			*posDataDst++ = xp + i;
			*posDataDst++ = yp;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;

			*posDataDst++ = xp + i;
			*posDataDst++ = yp + sample.driverWaitsLock * 4;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;

			yp += sample.driverWaitsLock * 4;

			// Driver waiting for render
			*posDataDst++ = xp + i;
			*posDataDst++ = yp;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;

			*posDataDst++ = xp + i;
			*posDataDst++ = yp + sample.driverWaitsRender * 4;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;

			yp += sample.driverWaitsRender * 4;

			// Driver waiting for swap
			*posDataDst++ = xp + i;
			*posDataDst++ = yp;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;

			*posDataDst++ = xp + i;
			*posDataDst++ = yp + sample.driverWaitsSwap * 4;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 1.0f;
			*posDataDst++ = 0.0f;
			*posDataDst++ = 1.0f;
		}

		// Lines at 16 and 32ms
		*posDataDst++ = xp;
		*posDataDst++ = 16 * 4;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 0.0f;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 1.0f;

		*posDataDst++ = xp + mSamplesToRecord;
		*posDataDst++ = 16 * 4;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 0.0f;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 1.0f;

		*posDataDst++ = xp;
		*posDataDst++ = 32 * 4;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 0.0f;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 1.0f;

		*posDataDst++ = xp + mSamplesToRecord;
		*posDataDst++ = 32 * 4;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 0.0f;
		*posDataDst++ = 1.0f;
		*posDataDst++ = 1.0f;

		mProfileLines->finishUpdate(mProfileTimeSamples.size() * 5 + 2);

		renderModelImmediate(*mProfileLines, false);
#endif
	}

	void RenderSystem::setupRenderMeshInstance(MeshInstance* meshInstance, VertexBufferRenderCommand const& renderCmd, uint64_t sortKey, uint64_t* currentProgramKey, vector<uint64_t>* currentTextureKeys, Material** currentMaterial)
	{
		// Mask off program and see if it has changed from previous.
		uint64_t thisProgramKey = sortKey;
		thisProgramKey >>= MPP_RENDER_SORT_PROGRAM_BITS_OFFSET;
		thisProgramKey &= ((1 << MPP_RENDER_SORT_PROGRAM_BITS_SIZE) - 1);

		// Set program
		bool programChanged = false;
		if (thisProgramKey != *currentProgramKey)
		{
			auto program = mResourceMgr->getProgramBySortId((uint32_t)thisProgramKey);
			setUsedProgram(program);

			*currentProgramKey = thisProgramKey;
			programChanged = true;
			mRenderInfo.programSwitches++;
		}

		// Set uniforms if the program or material have changed.
		auto material = static_cast<Material*>(renderCmd.material.get());
		if (programChanged || material != *currentMaterial)
		{
			material->setUniforms();
		}

		*currentMaterial = material;

		// Bind every material sampler. Texture keys are intentionally kept outside
		// the packed sort key: PBR needs more than the legacy two texture units.
		const size_t textureCount = (size_t)material->getNumTextures();
		if (textureCount > mCaps.maxFragmentTextureUnits)
		{
			THROW_MPP("Material requires more fragment texture units than supported by this renderer.", __LINE__, __FILE__, __func__);
		}
		if (currentTextureKeys->size() < textureCount)
		{
			currentTextureKeys->resize(textureCount, 0);
		}
		for (size_t i = 0; i < textureCount; ++i)
		{
			ResourcePtr textureResource = i < renderCmd.textures.size() && renderCmd.textures[i]
				? renderCmd.textures[i]
				: material->getTexture((int)i);

			// Pipeline-owned samplers are authoritative for matching shader names.
			// This generalizes PBR IBL binding and will also bind generic shadow
			// maps without making them material-owned texture slots.
			auto program = static_cast<Program*>(material->getProgram().get());
			auto const& samplerName = program->getSamplerName((int)i);
			if (samplerName == "SHADOW_MAP" && mActiveShadowDepthTarget)
			{
				static_cast<RenderTexture*>(mActiveShadowDepthTarget.get())->bindDepth((uint32_t)i);
				(*currentTextureKeys)[i] = (uint64_t)(uintptr_t)mActiveShadowDepthTarget.get();
				mRenderInfo.textureSwitches++;
				continue;
			}
			auto pipelineSampler = mActivePipelineSamplerOverrides.find(samplerName);
			if (pipelineSampler != mActivePipelineSamplerOverrides.end() && pipelineSampler->second)
			{
				textureResource = pipelineSampler->second;
			}
			auto texture = static_cast<Texture*>(textureResource.get());
			const uint64_t textureKey = (uint64_t)(uintptr_t)texture;
			if ((*currentTextureKeys)[i] != textureKey)
			{
				texture->bind((uint32_t)i);
				(*currentTextureKeys)[i] = textureKey;
				mRenderInfo.textureSwitches++;
			}
		}

		// Bind mesh material (including program), and vertex buffers
		meshInstance->mwMesh->bind(true);

		// Go through all mesh uniforms and set them.
		meshInstance->bindUniforms();

		// Wireframe?
		GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, meshInstance->mWireframe ? GL_LINE : GL_FILL));

		// Blend?
		if (meshInstance->mBlend)
		{
			GL_CHECK(glEnable(GL_BLEND));
			GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
			GL_CHECK(glDepthMask(GL_FALSE));
		}
	}

	void RenderSystem::teardownRenderMeshInstance(MeshInstance* meshInstance)
	{
		// Unbind
		meshInstance->mwMesh->bind(false);

		GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

		if (meshInstance->mBlend)
		{
			GL_CHECK(glDisable(GL_BLEND));
			GL_CHECK(glDepthMask(GL_TRUE));
		}
	}

	/*
	 * Render any meshes we have.
	 *
	 */
	void RenderSystem::flushVertexBuffers()
	{
		// Extract and sort list of render commands
		vector<SortedRenderCommand> renderCommands;

		for (size_t i = 0; i < mModelInstances->getCount(); ++i)
		{
			auto modelInstance = mModelInstances->getObject((uint32_t)i);
			auto const& modelMeshInstances = modelInstance->getMeshInstances();
		
			for (auto const& mi: modelMeshInstances)
			{
				if (mi->render())
				{
					// Fix up render commands here.  If there are none, then add a default one.
					if (mi->mRenderCommands.empty())
					{
						VertexBufferRenderCommand cmd{};
						mi->addRenderCommand(cmd);
					}

					for (auto const& renderCmd : mi->mRenderCommands)
					{
						// Create sort key
						uint64_t sortKey = 0;

						auto material = static_cast<Material*>(renderCmd.material.get());

						// Program. Textures are bound from their dynamic sampler list in
						// setupRenderMeshInstance rather than being limited to two keys.
						uint64_t programKey = (uint64_t)((Program*)material->getProgram().get())->getSortId();
						programKey <<= MPP_RENDER_SORT_PROGRAM_BITS_OFFSET;
						sortKey |= programKey;

						renderCommands.push_back({ sortKey,	renderCmd, mi });

						// Depth
						// modelMatrix is used to create final transform, so can use this
						// (Not used at the moment)
						/*
						auto modelMatrix = mi->mModelMatrix * mi->mLocalTransform;

						auto cameraPos = mi->mViewPos;
						auto modelPos = glm::vec3(modelMatrix[3]);

						float distanceToModel = glm::distance(cameraPos, modelPos);
						float distanceInScene = min(distanceToModel / mFarPlaneDistance, 1.0f);

						uint64_t distanceKey = 1 << MPP_RENDER_SORT_DEPTH_BITS_SIZE;
						distanceKey = (uint64_t)(distanceKey * distanceInScene);

						distanceKey <<= MPP_RENDER_SORT_DEPTH_BITS_OFFSET;
						sortKey |= distanceKey;
						*/
					}
				}
			}
		}

		// Sort in 3D mode.  In Orthographic/2D assume everything is specified in order.
		if (mProjectionType == ProjectionType::Perspective3D)
		{
			sort(renderCommands.begin(), renderCommands.end(), [](SortedRenderCommand const& a, SortedRenderCommand const& b) -> bool
			{
				bool const aTransparent = a.meshInstance->sortTransparent();
				bool const bTransparent = b.meshInstance->sortTransparent();
				if (aTransparent != bTransparent)
				{
					return !aTransparent; // Opaque and masked geometry first.
				}
				if (aTransparent)
				{
					auto const aPos = glm::vec3((a.meshInstance->mModelMatrix * a.meshInstance->mLocalTransform)[3]);
					auto const bPos = glm::vec3((b.meshInstance->mModelMatrix * b.meshInstance->mLocalTransform)[3]);
					auto const aDelta = aPos - a.meshInstance->mViewPos;
					auto const bDelta = bPos - b.meshInstance->mViewPos;
					return glm::dot(aDelta, aDelta) > glm::dot(bDelta, bDelta);
				}
				return a.key < b.key;
			});
		}

		// Now render all the meshes.
		uint64_t currentProgramKey = 0; // Sort ids start at 1, so this is guaranteed not to be one.
		vector<uint64_t> currentTextureKeys;

		Material* currentMaterial{ nullptr };
		for (auto const& renderCommand: renderCommands)
		{
			auto [key, cmd, meshInstance] = renderCommand;
			auto mesh = meshInstance->mwMesh;
			auto instanceCount = meshInstance->mInstanceCount;
			auto numPrimitives = mesh->getNumPrimitives();

			setupRenderMeshInstance(meshInstance, cmd, key, &currentProgramKey, &currentTextureKeys, &currentMaterial);

			auto count = cmd.count != ~0u ? cmd.count : numPrimitives;
			mesh->render(instanceCount, cmd.offset, count);
			mRenderInfo.primitivesRendered += (int)(count * instanceCount);
			mRenderInfo.batchCount++;

			teardownRenderMeshInstance(meshInstance);
		}

		mModelInstances->releaseAllObjects();
		mMeshInstances->releaseAllObjects();
	}
	
	/*
	 * Get rendering information about this frame.
	 *
	 */
	RenderInfo const& RenderSystem::getRenderInfo() const
	{
		return mRenderInfo;
	}

	void RenderSystem::addSceneFactory(string const& type, SceneFactory factory)
	{
		if (mSceneFactories.find(type) != mSceneFactories.end())
		{
			string errMsg = "Scene type: " + type + " already registered";
			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		mSceneFactories[type] = factory;
	}

	ScenePtr RenderSystem::createScene(string const& type)
	{
		auto it = mSceneFactories.find(type);
		if (it == mSceneFactories.end())
		{
			string errMsg = "Unknown scene type: " + type;
			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		return it->second(this);
	}

	void RenderSystem::renderScene(ScenePtr scene, CameraPtr camera, glm::vec2 const& offset2d, string const& pipelineName)
	{
		auto pipeline = getRenderPipeline(pipelineName);
		pipeline->render(scene, camera, offset2d);
	}

	RenderPipelinePtr RenderSystem::getOrCreateRenderPipeline(string const& name)
	{
		return getOrCreateRenderPipeline(name, {});
	}

	RenderPipelinePtr RenderSystem::getOrCreateRenderPipeline(string const& name, RenderPipelineOptions const& options)
	{
		auto it = mPipelines.find(name);
		if (it != mPipelines.end())
		{
			if (it->second->getOptions().mode != options.mode)
			{
				THROW_MPP("RenderPipeline '" + name + "' already exists with different options.", __LINE__, __FILE__, __func__);
			}
			return it->second;
		}

		auto pipeline = make_shared<RenderPipeline>(name, this, options);
		mPipelines[name] = pipeline;
		return pipeline;
	}

	RenderPipelinePtr RenderSystem::getRenderPipeline(string const& name)
	{
		auto it = mPipelines.find(name);

		if (it == mPipelines.end())
		{
			string errMsg = "RenderPipeline '" + name + "' not found.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __func__);
		}

		return it->second;
	}

	void RenderSystem::startStatsCollection()
	{
		mRenderInfo.clear();
	}

	/*
	 * Finish rendering.  Must be called before swapping screen.
	 *
	 */
	RenderInfo const& RenderSystem::finishStatsCollection()
	{
#ifdef MPP_PROFILE_BUILD
		mProfiler->sample();
#endif		
		// Display debug panel?
		if (mShowDebugPanel)
		{
			// Backup stats so we don't include them for the debug panel
			auto ri = mRenderInfo;

			renderDebugPanel();

			mRenderInfo = ri;
		}

		return mRenderInfo;
	}

} 