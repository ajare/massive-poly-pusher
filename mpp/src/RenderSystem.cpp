#if MPP_PLATFORM == MPP_PLATFORM_WINDOWS
#include <Windows.h>
#endif

#include <glew/glew.h>
#include <gl/gl.h>

#include <cassert>
#include <algorithm>
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
		, mShowDebugPanel(false)
		, mTimeUnit(TimeUnit::Milliseconds)
		, mSizeUnit(SizeUnit::Megabytes)
		, mInternalFont(nullptr)
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
		delete mModelInstances;
		delete mMeshInstances;
		delete mInternalFont;
		destroyLightsData();

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

		// Text settings
		mTextUniforms = make_shared<UniformCollection>();
		mTextUniforms->setUniform("COLOUR", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		mTextParams = make_shared<ModelRenderParams>();
		mTextParams->setModelUniforms(mTextUniforms);
		mTextParams->setModelPointSize(16.0f);
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
		ProgrammaticMaterialStream* defaultMatStream = new ProgrammaticMaterialStream(mResourceMgr);
		
		defaultMatStream->setProgram(mDefaultProgram2d->getName());
		defaultMatStream->setTexture("TEX1", "__mpp_tex_none__");
		mDefaultMaterial = resourceMgr->declareResource("__mpp_mat_default__", mpp::ResourceStreamPtr(defaultMatStream)).first;
		addCoreResource(mDefaultMaterial, true);

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

			for (int i = 0; i < MaxTextGlyphs * 6; ++i)
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

			for (int i = 0; i < MaxTextGlyphs; ++i)
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

			for (int i = 0; i < MaxTextGlyphs * 6; ++i)
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

		// Render targets
		mSceneTarget = createRenderTexture("SceneTarget", getWindowWidth(), getWindowHeight(), 1, true);
		
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

		mScreen = RenderTargetPtr(new Screen(width, height));
		setRenderTarget(mScreen);
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

		mWindowWidth = mRenderTarget->getWidth();
		mWindowHeight = mRenderTarget->getHeight();
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
		auto rtStream = new ProgrammaticRenderTextureStream(mResourceMgr);

		rtStream->setTarget(TextureTarget::Texture2D);
		rtStream->setInternalFormat(TextureInternalType::UnsignedInteger, true, 8, 4);
		rtStream->setWidth(width);
		rtStream->setHeight(height);
		rtStream->setDepthBuffer(depthBuffer);
		rtStream->setNumAttachments(numAttachments);

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
			glm::vec2(mWindowWidth / 2.0f, mWindowHeight / 2.0f),
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
			if (offset + numChars > MaxTextGlyphs)
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
			if (offset + numChars * 6 > MaxTextGlyphs)
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
			if (offset + numChars > MaxTextGlyphs)
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
			if (offset + numChars * 6 > MaxTextGlyphs)
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
			int diffuseId = p->getUniformId("DIFFUSE");
			GL_CHECK(glUniform4f(diffuseId, 1, 1, 1, 1));
		}

		int mcpId = p->getModelCameraProjectionMatrixId();
		GL_CHECK(glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));

		int hwsId = p->getHalfWindowSizeId();
		GL_CHECK(glUniform2f(hwsId, mWindowWidth / 2.0f, mWindowHeight / 2.0f));

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
		translateTransform2d(glm::vec2(x, mWindowHeight - y));
		GL_CHECK(scaleTransform2d(glm::vec2(width / (float)mWindowWidth, height / (float)mWindowHeight)));

		int mcpId = p->getModelCameraProjectionMatrixId();
		GL_CHECK(glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));

		int hwsId = p->getHalfWindowSizeId();
		GL_CHECK(glUniform2f(hwsId, mWindowWidth / 2.0f, mWindowHeight / 2.0f));

		int diffuseId = p->getUniformId("DIFFUSE");
		GL_CHECK(glUniform4f(diffuseId, colour.red, colour.green, colour.blue, colour.alpha));

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

	/*
	 * Render any meshes we have.
	 *
	 */
	void RenderSystem::flushVertexBuffers()
	{
		// Extract and sort list of meshes
		vector<SortableMeshInstance> meshInstances;

		for (size_t i = 0; i < mModelInstances->getCount(); ++i)
		{
			auto modelInstance = mModelInstances->getObject((uint32_t)i);
			auto const& modelMeshInstances = modelInstance->getMeshInstances();
		
			for (auto const& mi: modelMeshInstances)
			{
				if (mi->render())
				{
					// Create sort key
					uint64_t sortKey = 0;

					auto material = static_cast<Material*>(mi->getMaterial().get());
					auto numTextures = material->getNumTextures();

					// Texture 0
					if (numTextures > 0)
					{
						auto texture = static_cast<Texture*>(mi->getTexture(0).get());
						uint64_t textureKey = (uint64_t)texture->getSortId();
						textureKey <<= MPP_RENDER_SORT_TEXTURE0_BITS_OFFSET;
						sortKey |= textureKey;
					}

					// Texture 1
					if (numTextures > 1)
					{
						auto texture = static_cast<Texture*>(mi->getTexture(1).get());
						uint64_t textureKey = (uint64_t)texture->getSortId();
						textureKey <<= MPP_RENDER_SORT_TEXTURE1_BITS_OFFSET;
						sortKey |= textureKey;
					}

					// Program
					uint64_t programKey = (uint64_t)((Program*)material->getProgram().get())->getSortId();
					programKey <<= MPP_RENDER_SORT_PROGRAM_BITS_OFFSET;
					sortKey |= programKey;

					meshInstances.push_back(make_pair(sortKey, mi));

					// Depth
					// modelMatrix is used to create final transform, so can use this
					auto modelMatrix = mi->mModelMatrix * mi->mLocalTransform;
					
					auto cameraPos = mi->mViewPos;
					auto modelPos = glm::vec3(modelMatrix[3]);

					float distanceToModel = glm::distance(cameraPos, modelPos);
					float distanceInScene = min(distanceToModel / mFarPlaneDistance, 1.0f);

					uint64_t distanceKey = 1 << MPP_RENDER_SORT_DEPTH_BITS_SIZE;
					distanceKey = (uint64_t)(distanceKey * distanceInScene);

					distanceKey <<= MPP_RENDER_SORT_DEPTH_BITS_OFFSET;
					sortKey |= distanceKey;
				}
			}
		}

		// Sort in 3D mode.  In Orthographic/2D assume everything is specified in order.
		if (mProjectionType == ProjectionType::Perspective3D)
		{
			sort(meshInstances.begin(), meshInstances.end(), [](SortableMeshInstance const& a, SortableMeshInstance const& b) -> bool
			{
				return a.first < b.first;
			});
		}

		// Now render all the meshes.
		uint64_t currentProgramKey = 0; // Sort ids start at 1, so this is guaranteed not to be one.
		uint64_t currentTexture0Key = 0, currentTexture1Key = 0;

		Material* currentMaterial{ nullptr };
		for (auto meshInstance: meshInstances)
		{
			// Mask off program and see if it has changed from previous.
			uint64_t thisProgramKey = meshInstance.first;
			thisProgramKey >>= MPP_RENDER_SORT_PROGRAM_BITS_OFFSET;
			thisProgramKey &= ((1 << MPP_RENDER_SORT_PROGRAM_BITS_SIZE) - 1);

			// Set program
			bool programChanged = false;
			if (thisProgramKey != currentProgramKey)
			{
				auto program = mResourceMgr->getProgramBySortId((uint32_t)thisProgramKey);
				setUsedProgram(program);

				currentProgramKey = thisProgramKey;
				programChanged = true;
				mRenderInfo.programSwitches++;
			}

			// Set uniforms if the program or material have changed.
			auto material = static_cast<Material*>(meshInstance.second->mMaterial.get());
			if (programChanged || material != currentMaterial)
			{
				material->setUniforms();
			}

			currentMaterial = material;

			// Mask off textures and see if they have changed from previous.
			uint64_t thisTexture0Key = meshInstance.first;
			thisTexture0Key >>= MPP_RENDER_SORT_TEXTURE0_BITS_OFFSET;
			thisTexture0Key &= ((1 << MPP_RENDER_SORT_TEXTURE0_BITS_SIZE) - 1);

			if (thisTexture0Key > 0 && (thisTexture0Key != currentTexture0Key || programChanged))
			{
				auto texture = static_cast<Texture*>(mResourceMgr->getTextureBySortId((uint32_t)thisTexture0Key).get());
				texture->bind(0);

				currentTexture0Key = thisTexture0Key;
				mRenderInfo.textureSwitches++;
			}

			uint64_t thisTexture1Key = meshInstance.first;
			thisTexture1Key >>= MPP_RENDER_SORT_TEXTURE1_BITS_OFFSET;
			thisTexture1Key &= ((1 << MPP_RENDER_SORT_TEXTURE1_BITS_SIZE) - 1);

			if (thisTexture1Key > 0 && (thisTexture1Key != currentTexture1Key || programChanged))
			{
				auto texture = static_cast<Texture*>(mResourceMgr->getTextureBySortId((uint32_t)thisTexture1Key).get());
				texture->bind(1);

				currentTexture1Key = thisTexture1Key;
				mRenderInfo.textureSwitches++;
			}

			// Bind mesh material (including program), and vertex buffers
			meshInstance.second->mwMesh->bind(true);

			// Go through all mesh uniforms and set them.
			meshInstance.second->bindUniforms();

			// Wireframe?
			GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, meshInstance.second->mWireframe ? GL_LINE : GL_FILL));

			// Blend?
			if (meshInstance.second->mBlend)
			{
				GL_CHECK(glEnable(GL_BLEND));
				GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
			}

			// Render
			auto const& renderRanges = meshInstance.second->mRenderRanges;
			if (renderRanges.empty())
			{
				meshInstance.second->mwMesh->render(meshInstance.second->mInstanceCount);
				mRenderInfo.primitivesRendered += (int)(meshInstance.second->mwMesh->getNumPrimitives() * meshInstance.second->mInstanceCount);
			}
			else
			{
				for (auto const& range: renderRanges)
				{
					auto count = range.second != (size_t)-1 ? range.second : meshInstance.second->mwMesh->getNumPrimitives();
					meshInstance.second->mwMesh->render(meshInstance.second->mInstanceCount, range.first, count);
					mRenderInfo.primitivesRendered += (int)(count * meshInstance.second->mInstanceCount);
				}
			}

			mRenderInfo.batchCount++;

			// Unbind
			meshInstance.second->mwMesh->bind(false);

			GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

			if (meshInstance.second->mBlend)
			{
				GL_CHECK(glDisable(GL_BLEND));
			}
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
		auto it = mPipelines.find(name);
		if (it != mPipelines.end())
		{
			return it->second;
		}
		else
		{
			auto rs = make_shared<RenderPipeline>(name, this);
			mPipelines[name] = rs;
			return rs;
		}
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