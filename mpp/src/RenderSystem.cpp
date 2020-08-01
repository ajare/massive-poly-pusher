#if MPP_PLATFORM == MPP_PLATFORM_WIN32
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
#include "mpp/StringProgramStream.h"
#include "mpp/Texture.h"
#include "mpp/TextureStream.h"
#include "mpp/Model.h"
#include "mpp/ModelStream.h"
#include "mpp/ProgrammaticModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/RenderTexture.h"
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
	RenderSystem::RenderSystem(int windowWidth, int windowHeight)
		: mLogger(nullptr)
		, mWindowWidth(windowWidth)
		, mWindowHeight(windowHeight)
		, mResourceMgr(nullptr)
		, mClearColour(0.0f, 0.0f, 0.0f, 1.0f)
		, mwActiveProgram(nullptr)
#ifdef MPP_PROFILE_BUILD
		, mProfiler(nullptr)
		, mProfileLines(nullptr)
		, mSamplesToRecord(300)
#endif
		, mRenderTarget(nullptr)
		, mScreen(nullptr)
		, mProjectionType(ProjectionType::Unknown)
		, mShowDebugPanel(false)
		, mTimeUnit(TimeUnit::Milliseconds)
		, mSizeUnit(SizeUnit::Megabytes)
	{
		mLogger = new Logger();
		if (!mLogger->initialise("mpp.log"))
		{
			THROW_MPP("Could not initialise RenderSystem logger", __LINE__, __FILE__, __FUNCTION__);
		}

		initialise();
	}

	/*
	 * Destructor.
	 *
	 */
	RenderSystem::~RenderSystem()
	{
#ifdef MPP_PROFILE_BUILD
		delete mProfiler;
		delete mProfileLines;
#endif

#ifdef MPP_DEBUG_BUILD
		delete mStackWalker;
#endif

		delete mLogger;
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
		renderSystem->logMessage(strMsg);

#ifdef MPP_DEBUG_BUILD
		RenderSystem::OpenGLError::Severity svrty;
		switch (severity)
		{
		case GL_DEBUG_SEVERITY_HIGH_ARB:
			svrty = RenderSystem::OpenGLError::Severity::High;
			THROW_MPP("OpenGL error caught: check log.", __LINE__, __FILE__, __FUNCTION__);
			break;

		case GL_DEBUG_SEVERITY_MEDIUM_ARB:
			svrty = RenderSystem::OpenGLError::Severity::Medium;
			break;

		case GL_DEBUG_SEVERITY_LOW_ARB:
			svrty = RenderSystem::OpenGLError::Severity::Low;
			break;

		default:
			svrty = RenderSystem::OpenGLError::Severity::High;
			THROW_MPP("OpenGL error caught: check log.", __LINE__, __FILE__, __FUNCTION__);
			break;
		}

		//renderSystem->addOpenGLError(message, svrty);
		renderSystem->debugStackTrace();
#endif
	}

	/*
	 * Load core resources, eg after recreating screen.
	 *
	 */
	void RenderSystem::_loadCoreResources()
	{
		mNoTexture->load();
		
		mInternalFontTexture->load();
		mInternalFont->setTexture(mInternalFontTexture);

		mTextMesh->load();
		mColouredTextMesh->load();
		mFullscreenQuad->load();
	}

	/*
	 * Unload core resources, eg before recreating screen.
	 *
	 */
	void RenderSystem::_unloadCoreResources()
	{
		mNoTexture->unload();

		mInternalFontTexture->unload();
		mInternalFont->setTexture(nullptr);

		mTextMesh->unload();
		mColouredTextMesh->unload();
		mFullscreenQuad->unload();
	}

	/*
	 * Get window width.
	 *
	 */
	int RenderSystem::getWindowWidth() const
	{
		return mWindowWidth;
	}

	/*
	 * Get window height.
	 *
	 */
	int RenderSystem::getWindowHeight() const
	{
		return mWindowHeight;
	}

	/*
	 * Get window aspect ratio.
	 *
	 */
	float RenderSystem::getAspectRatio() const
	{
		return (float)getWindowWidth() / (float)getWindowHeight();
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
			THROW_MPP(msg, __LINE__, __FILE__, __FUNCTION__);
		}

		checkExtensions();
		checkCaps();

#ifdef MPP_PROFILE_BUILD
		mProfiler = new Profiler();
#endif

#ifdef MPP_DEBUG_BUILD
		mStackWalker = new DebugStackWalker(this);
#endif

		setDefaultState();
		setDisplay(mWindowWidth, mWindowHeight);
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
	 * To be called before deleting a context (eg when resizing screen).
	 *
	 */
	void RenderSystem::preContextDeletion()
	{
		_unloadCoreResources();

#ifdef MPP_PROFILE_BUILD
		delete mProfiler;
		mProfiler = nullptr;
		
		delete mProfileLines;
		mProfileLines = nullptr;
#endif
	}

	/*
	 * To be called after creating a context (eg when resizing screen).
	 *
	 */
	void RenderSystem::postContextCreation(int windowWidth, int windowHeight)
	{
#ifdef MPP_PROFILE_BUILD
		mProfiler = new Profiler();

		mProfileLines = new LineBatch(
			"ProfileLines",
			Batch::ColourOptions::FloatRGBA,
			false,
			mSamplesToRecord * 5 + 2, // 5 seconds + 2 extra for graph
			this,
			mResourceMgr); 

		mProfileLines->load();
#endif

		_loadCoreResources();
		setDefaultState();
		setDisplay(windowWidth, windowHeight);
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
			THROW_MPP("OpenGL 3.2 was not found.", __LINE__, __FILE__, __FUNCTION__);
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
				logMessage("GL_ARB_debug_output initialised.");
			}
			else if (extensionName == "GL_EXT_texture_rectangle")
			{
				logMessage("GL_EXT_texture_rectangle initialised.");
			}
			else if (extensionName == "GLEW_ARB_buffer_storage")
			{
				logMessage("GLEW_ARB_buffer_storage initialised.");
			}
			else if (extensionName == "GLEW_ARB_map_buffer_range")
			{
				logMessage("GLEW_ARB_map_buffer_range initialised.");
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
			THROW_MPP("Could not parse GL_VERSION: " + glVersion, __LINE__, __FILE__, __FUNCTION__);
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
			THROW_MPP("Could not parse GL_SHADING_LANGUAGE_VERSION: " + glslVersion, __LINE__, __FILE__, __FUNCTION__);
		}

		logMessage("GL version: " + glVersion);
		logMessage("GLSL version: " + glslVersion);
		logMessage("Renderer: " + glRenderer);
		logMessage("Vendor: " + vendorInfo);
		logMessage("");

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
		GLint maxTextureUnits;
		GLint maxTextureSize;
		GLint maxRectTextureSize = 0;

		GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTextureUnits));
		GL_CHECK(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize));

		if (glewIsSupported("GL_EXT_texture_rectangle"))
		{
			GL_CHECK(glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE_EXT, &maxRectTextureSize));
		}

		mCaps.maxTextureUnits = maxTextureUnits;
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

		// Print caps
		logMessage(utils::StringUtils::format("Supported point size range: {} to {}", mCaps.pointSizeRange[0], mCaps.pointSizeRange[1]));
		logMessage(utils::StringUtils::format("Supported aliased line width range: %2.1 to {}", mCaps.aliasedLineWidthRange[0], mCaps.aliasedLineWidthRange[1]));
		logMessage(utils::StringUtils::format("Supported texture units: {}", mCaps.maxTextureUnits));
		logMessage(utils::StringUtils::format("Supported square texture size: {}x{}", mCaps.maxTextureSize, mCaps.maxTextureSize));
		logMessage(utils::StringUtils::format("Supported non-square texture size: {}x{}", mCaps.maxRectTextureSize, mCaps.maxRectTextureSize));
		logMessage(utils::StringUtils::format("Depth range: {} to {}", mCaps.depthRange[0], mCaps.depthRange[1]));
		logMessage(utils::StringUtils::format("Max recommended elements: {}", mCaps.maxRecommendedElements));
		logMessage(utils::StringUtils::format("Max recommended vertices: {}", mCaps.maxRecommendedVertices));
		logMessage(utils::StringUtils::format("Streaming geometry: {}", mCaps.streamingGeometry ? "yes" : "no"));

		logMessage("");
	}

	/*
	 * Create core resources which the RenderSystem needs to operate on,
	 * outside of the client-specific resources.
	 *
	 */
	void RenderSystem::createCoreResources(ResourceManager* resourceMgr)
	{
		mResourceMgr = resourceMgr;

		mNoTexture = resourceMgr->getResource("__mpp_tex_none__");
		
		// Default 2d program
		mesh::MeshSpecification spec2d(mesh::Primitive::Type::Triangles);
		auto layout = spec2d.createVertexBufferAttributeLayout();
		layout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
		layout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::UnsignedByte, true);

		mDefaultProgram2d = resourceMgr->getOrCreateDefault2dProgram(spec2d, 0, true);

		// Default 3d program
		mDefaultProgram3d = resourceMgr->getResource("__mpp_p3d_tris_p3n3t2c4__");

		mInternalFontTexture = resourceMgr->getResource("__mpp_tex_internalfont__");
		mInternalFont = new Font(mInternalFontTexture);

		// Default material
		ProgrammaticMaterialStream* defaultMatStream = new ProgrammaticMaterialStream();
		
		defaultMatStream->setProgram(mDefaultProgram2d->getName());
		defaultMatStream->setTexture("TEX1", "__mpp_tex_none__");
		resourceMgr->createResource<mpp::Material>("__mpp_mat_default__", mpp::ResourceStreamPtr(defaultMatStream))->load();

		// Internal font
		bool textAsPoints = mCaps.pointSizeRange[1] >= 16.0f;
		ProgrammaticMaterialStream* textMatStream = new ProgrammaticMaterialStream();

		textMatStream->setProgram(textAsPoints ? "__mpp_p2d_points_text__" : "__mpp_p2d_tris_text__");
		textMatStream->setFloatUniform("COLOUR", glm::vec4(1, 1, 1, 1));
		textMatStream->setTexture("TEX1", "__mpp_tex_internalfont__");
		resourceMgr->createResource<mpp::Material>("__mpp_mat_text_pt__", mpp::ResourceStreamPtr(textMatStream))->load();

		ProgrammaticMaterialStream* textMatStreamColoured = new ProgrammaticMaterialStream();
		textMatStreamColoured->setProgram(textAsPoints ? "__mpp_p2d_points_text_coloured__" : "__mpp_p2d_tris_text_coloured__");
		textMatStreamColoured->setFloatUniform("COLOUR", glm::vec4(1, 1, 1, 1));
		textMatStreamColoured->setTexture("TEX1", "__mpp_tex_internalfont__");
		resourceMgr->createResource<mpp::Material>("__mpp_mat_text_ptc__", mpp::ResourceStreamPtr(textMatStreamColoured))->load();

		int fontTextureWidth = ((Texture&)*mInternalFontTexture).getWidth();
		int fontTextureHeight = ((Texture&)*mInternalFontTexture).getHeight();
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
				(uint8)i,
				(i % 16) * 16,
				fontTextureHeight - 16 - (i / 16) * 16,
				glyphWidth,
				glyphHeight,
				kern,
				0);
		}

		// Create font mesh
		ProgrammaticModelStream* textStream = new ProgrammaticModelStream();

		int glyphCount = 2048;

		if (textAsPoints)
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Points);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout();

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);

			int textMesh = textStream->createMesh("0", textSpec, "__mpp_mat_text_pt__", 32, 16.0f);

			for (int i = 0; i < glyphCount; ++i)
			{
				textStream->addVertexData<float>(textMesh, { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f });
			}
		}
		else
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Triangles);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout();

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);

			int textMesh = textStream->createMesh("0", textSpec, "__mpp_mat_text_pt__", 32, -1.0f);

			for (int i = 0; i < glyphCount * 6; ++i)
			{
				textStream->addVertexData<float>(textMesh, { 0.0f, 0.0f, 0.0f, 0.0f });
			}
		}

		auto textStreamPtr = ResourceStreamPtr(textStream);
		auto textRes = resourceMgr->createResource<Model>("__mpp_internal_text_mesh__", textStreamPtr);
		textRes->load();

		mTextMesh = resourceMgr->getResource("__mpp_internal_text_mesh__");

		// Coloured font mesh
		textStream = new ProgrammaticModelStream();

		if (textAsPoints)
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Points);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout();

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord4, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

			int textMesh = textStream->createMesh("0", textSpec, "__mpp_mat_text_ptc__", 32, 16.0f);

			for (int i = 0; i < glyphCount; ++i)
			{
				textStream->addVertexData<float>(textMesh, { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}
		else
		{
			mesh::MeshSpecification textSpec(mesh::Primitive::Type::Triangles);
			textSpec.setStorageType(mesh::VertexBufferStorageType::Dynamic);
			textSpec.setIndexedVertices(false);

			mesh::VertexBufferAttributeLayout* attribLayout = textSpec.createVertexBufferAttributeLayout();

			attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
			attribLayout->createAttribute(mesh::Vertex::Component::Colour4, mesh::Vertex::DataType::Float, true);

			int textMesh = textStream->createMesh("0", textSpec, "__mpp_mat_text_ptc__", 32, -1.0f);

			for (int i = 0; i < glyphCount * 6; ++i)
			{
				textStream->addVertexData<float>(textMesh, { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		textStreamPtr = ResourceStreamPtr(textStream);
		textRes = resourceMgr->createResource<Model>("__mpp_internal_coloured_text_mesh__", textStreamPtr);
		textRes->load();

		mColouredTextMesh = resourceMgr->getResource("__mpp_internal_coloured_text_mesh__");

		// Fullscreen mesh
		ProgrammaticModelStream* quadStream = new ProgrammaticModelStream();
		mesh::MeshSpecification quadSpec = mesh::MeshSpecification(mesh::Primitive::Type::Triangles);

		auto attribLayout = quadSpec.createVertexBufferAttributeLayout();
		attribLayout->createAttribute(mesh::Vertex::Component::Position2, mesh::Vertex::DataType::Float, false);
		attribLayout->createAttribute(mesh::Vertex::Component::TexCoord2, mesh::Vertex::DataType::Float, false);
		quadSpec.setStorageType(mesh::VertexBufferStorageType::Static);
		quadSpec.setIndexedVertices(false);

		auto fullscreenProgram = resourceMgr->getOrCreateDefault2dProgram(quadSpec, 0, true);
		auto quadMesh = quadStream->createMesh("0", quadSpec, fullscreenProgram->getName(), 32, -1.0f);

		quadStream->addVertexData<float>(quadMesh, { 0.0f, 0.0f, 0.0f, 0.0f });
		quadStream->addVertexData<float>(quadMesh, { (float)mWindowWidth, 0.0f, 1.0f, 0.0f });
		quadStream->addVertexData<float>(quadMesh, { (float)mWindowWidth, (float)mWindowHeight, 1.0f, 1.0f });
		quadStream->addVertexData<float>(quadMesh, { (float)mWindowWidth, (float)mWindowHeight, 1.0f, 1.0f });
		quadStream->addVertexData<float>(quadMesh, { 0.0f, (float)mWindowHeight, 0.0f, 1.0f });
		quadStream->addVertexData<float>(quadMesh, { 0.0f, 0.0f, 0.0f, 0.0f });

		resourceMgr->createResource<Model>("__mpp_mesh_fullscreen_quad__", ResourceStreamPtr(quadStream))->load();
		mFullscreenQuad = resourceMgr->getResource("__mpp_mesh_fullscreen_quad__");

		// Render targets
		mSceneTarget = createRenderTexture(getWindowWidth(), getWindowHeight(), 1, true);
		mFullscreenFxTarget = createRenderTexture(getWindowWidth(), getWindowHeight(), 1, false);

		// Blur textures: should use cascading sizes based on blur kernel size.
		mBlur1Target = createRenderTexture(getWindowWidth(), getWindowHeight(), 1, false);
		mBlur2Target = createRenderTexture(getWindowWidth(), getWindowHeight(), 1, false);
		
		// Set none as active
		mwActiveProgram = nullptr;

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

	/*
	 * Use the default program.
	 *
	 */
	void RenderSystem::useDefaultProgram()
	{
		switch (mProjectionType)
		{
		case ProjectionType::Perspective3D:
			setUsedProgram((Program*)mDefaultProgram3d.get());
			break;

		case ProjectionType::Ortho2D:
			setUsedProgram((Program*)mDefaultProgram2d.get());
			break;

		default:
			THROW_MPP("Unsupported projection.", __LINE__, __FILE__, __FUNCTION__);
		}
	}

	/*
	 * Set the used program.  This should only be called
	 * after GL has accepted the program as active.
	 *
	 */
	void RenderSystem::setUsedProgram(Program* program)
	{
		if (program == mwActiveProgram)
		{
			return;
		}

		program->bind();
		mwActiveProgram = program;
	}

	/*
	 * Get used program.
	 *
	 */
	Program* RenderSystem::getUsedProgram()
	{
		return mwActiveProgram;
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
		GL_CHECK(glEnable(GL_DEPTH_TEST));
		GL_CHECK(glDepthFunc(GL_LESS));

		GL_CHECK(glDisable(GL_PROGRAM_POINT_SIZE));

		// Set matrices to identity
		m3dCameraMatrix = glm::mat4();
		m3dCameraMatrix = glm::inverse(m3dCameraMatrix);
		m3dProjectionMatrix = glm::mat4();
		m3dModelMatrix = glm::mat4();
		m3dModelCameraProjectionMatrix = glm::mat4();

		mClearColour = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	/*
	 * Set up the display/viewport.
	 *
	 */
	void RenderSystem::setDisplay(int width, int height)
	{
		mWindowWidth = width;
		mWindowHeight = height;

		mScreen = RenderTargetPtr(new Screen(width, height));
		setRenderTarget(mScreen);
	}

	/*
	 * Update the current render target.
	 *
	 */
	void RenderSystem::updateDisplay(int width, int height)
	{
		mWindowWidth = width;
		mWindowHeight = height;

		mRenderTarget->setDimensions(width, height);
	}

	/*
	 * Write a message to logfile.
	 *
	 */
	void RenderSystem::logMessage(string const& message)
	{
		mLogger->message(message);
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
	RenderTargetPtr RenderSystem::createRenderTexture(int width, int height, int numAttachments, bool depthBuffer)
	{
		return RenderTargetPtr(new RenderTexture(width, height, numAttachments, depthBuffer, this));
	}

	/*
	 * Create (and return) a texture tile.
	 *
	 */
	TextureTile const& RenderSystem::createTextureTile(string const& name, ResourcePtr texture, int offX, int offY, float u0, float v0, float u1, float v1)
	{
		if (mTextureTiles.find(name) != mTextureTiles.end())
		{
			string errMsg = "Texture tile named '" + name + "' already exists.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __FUNCTION__);
		}

		mTextureTiles[name] = TextureTile(texture, u0, v0, u1, v1);
		return mTextureTiles[name];
	}

	/*
	 * Delete the named texture tile
	 *
	 */
	void RenderSystem::destroyTextureTile(string const& name)
	{
		auto it = mTextureTiles.find(name);
		if (it == mTextureTiles.end())
		{
			string errMsg = "Texture tile named '" + name + "' does not exist.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __FUNCTION__);
		}

		mTextureTiles.erase(it);
	}

	/*
	 * Return the named texture tile.
	 *
	 */
	TextureTile const& RenderSystem::getTextureTile(std::string const& name) const
	{
		if (mTextureTiles.find(name) == mTextureTiles.end())
		{
			string errMsg = "Texture tile named '" + name + "' does not exist.";
			THROW_MPP(errMsg, __LINE__, __FILE__, __FUNCTION__);
		}

		return mTextureTiles.at(name);
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
			GL_CHECK(glScissor(0, 0, mRenderTarget->getWidth(), mRenderTarget->getHeight()));
		}
		else
		{
			ClipRectangle const& cr = mClipStack.top();
			GL_CHECK(glScissor(cr.x, cr.y, cr.width, cr.height));
		}
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
		m3dCameraInverseMatrix = glm::inverse(m3dCameraMatrix);
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
		m3dCameraInverseMatrix = glm::inverse(m3dCameraMatrix);

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
		m3dCameraInverseMatrix = glm::inverse(m3dCameraMatrix);
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
		m3dCameraInverseMatrix = glm::inverse(m3dCameraMatrix);
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

	/*
	 * Clear the screen to the specified colour.
	 *
	 */
	void RenderSystem::clearScreen(Colour const& colour)
	{
		GL_CHECK(glClearColor(colour.red, colour.green, colour.blue, colour.alpha));
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	}

	/*
	 * Render a model.
	 *
	 */
	ModelInstance* RenderSystem::renderModelBatched(Model const& model, bool alphaBlend, UniformCollection const* uniforms, uint32 primitiveCount)
	{
		ModelInstance* mi = new ModelInstance(model, 
			m3dModelCameraProjectionMatrix, 
			getNormalMatrix(), 
			glm::vec2(mWindowWidth / 2.0f, mWindowHeight / 2.0f));

		if (uniforms)
		{
			mi->setUniformCollection((UniformCollection const&)*uniforms);
		}

		auto& instances = mi->getMeshInstances();
		for (auto& instance: instances)
		{
			instance->setRenderCount(primitiveCount);
			instance->blend(alphaBlend);
		}

		mModelInstances.push_back(mi);

		return mi;
	}

	/*
	 * Render a model.
	 *
	 */
	void RenderSystem::renderModelImmediate(Model const& model, bool alphaBlend, UniformCollection const* uniforms, uint32 primitiveCount)
	{
		renderModelBatched(model, alphaBlend, uniforms, primitiveCount);
		flushVertexBuffers();
	}

	/*
	 * Set up the model used for rendering text
	 *
	 */
	int RenderSystem::buildTextVertexBuffer(VertexBuffer* buffer, string const& text, int& offset, int x, int y)
	{
		char const* textPtr = text.c_str();
		int numChars = strlen(textPtr);

		int vertexStride = buffer->getVertexStride() / sizeof(float);

		vector<int8>& bufferData = buffer->getBufferData();
		float* bufferPtr = (float*)&(bufferData[offset * vertexStride * sizeof(float)]);

		float xpos = (float)x + 8; // 8 to offset default kerning
		float ypos = (float)y;

		if (mCaps.pointSizeRange[1] >= 16)
		{
			if (offset + numChars > 2048)
			{
				return 0;
			}

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
			if (offset + numChars * 6 > 2048)
			{
				return 0;
			}

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
		int numChars = strlen(textPtr);

		int vertexStride = buffer->getVertexStride() / sizeof(float);

		vector<int8>& bufferData = buffer->getBufferData();
		float* bufferPtr = (float*)&(bufferData[offset * vertexStride * sizeof(float)]);

		float xpos = (float)x + 8; // 8 to offset default kerning
		float ypos = (float)y;

		Colour colour = Colour::White;

		if (mCaps.pointSizeRange[1] >= 16)
		{
			if (offset + numChars > 2048)
			{
				return 0;
			}

			int i = 0, numSpecial = 0;
			while (i < numChars)
			{
				// Format for colour is [#RRGGBBAA]
				// Colours are hex, and should be converted to uint8
				// Then change text spec from float to uint8
				// Pack the value into a uint32 and reinterpret_cast to float
				if (i < (numChars - 8) && textPtr[i] == '[' && 
					textPtr[i + 1] == '#' && textPtr[i + 10] == ']')
				{
					uint8 tcolour[4] = { 0, 0, 0, 0 };
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
			if (offset + numChars * 6 > 2048)
			{
				return 0;
			}

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

	/*
	 * Add a post-process effect.
	 *
	 */
	void RenderSystem::addPostEffect(string const& material, UniformCollection const& uniforms, int attachment, BlendMode srcBlend, BlendMode dstBlend)
	{
		PostProcessEffect ppe;

		ppe.material = material;
		ppe.uniforms = uniforms;
		ppe.attachment = attachment;
		ppe.blendSrc = srcBlend;
		ppe.blendDst = dstBlend;

		mPostProcessEffects.push_back(ppe);
	}

	/*
	 * Render a fullscreen quad, for effects.
	 *
	 */
	void RenderSystem::renderFullscreenQuad(ResourcePtr material, UniformCollection* uniforms)
	{
		flushVertexBuffers();

		Material* m = (Material*)material.get();

		// Set program
		Program* p = (Program*)m->getProgram().get();
		setUsedProgram(p);
		mRenderInfo.programSwitches++;

		// Set uniforms
		m->setUniforms();

		if (uniforms)
		{
			uniforms->bindUniforms(p);
		}

		int mcpId = p->getModelCameraProjectionMatrixId();
		GL_CHECK(glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));

		int hwsId = p->getHalfWindowSizeId();
		GL_CHECK(glUniform2f(hwsId, mWindowWidth / 2.0f, mWindowHeight / 2.0f));

		// Set texture
		for (int i = 0; i < m->getNumTextures(); ++i)
		{
			((Texture*)m->getTexture(i).get())->bind(i);
			mRenderInfo.textureSwitches++;
		}

		// Bind mesh
		auto quadMesh = ((Model*)mFullscreenQuad.get())->getMesh(0);
		quadMesh->bind(true);
		quadMesh->render();

		// Unbind
		quadMesh->bind(false);

		mRenderInfo.batchCount++;
		mRenderInfo.fullscreenQuads++;
	}

	/*
	 * Render a rendertexture as a fullscreen quad
	 *
	 */
	void RenderSystem::renderFullscreenQuad(RenderTexture* texture, int attachment, BlendMode srcBlend, BlendMode dstBlend, UniformCollection* uniforms)
	{
		flushVertexBuffers();

		// Set program
		Program* p = (Program*)(mResourceMgr->getResource("__mpp_p2d_fullscreen__").get());

		setUsedProgram(p);
		mRenderInfo.programSwitches++;

		// Set uniforms
		if (uniforms)
		{
			uniforms->bindUniforms(p);
		}

		int mcpId = p->getModelCameraProjectionMatrixId();
		GL_CHECK(glUniformMatrix4fv(mcpId, 1, GL_FALSE, glm::value_ptr(m3dModelCameraProjectionMatrix)));

		int hwsId = p->getHalfWindowSizeId();
		GL_CHECK(glUniform2f(hwsId, mWindowWidth / 2.0f, mWindowHeight / 2.0f));

		int diffuseId = p->getUniformId("DIFFUSE");
		GL_CHECK(glUniform4f(diffuseId, 1, 1, 1, 1));

		// Set texture
		texture->bind(attachment, 0);
		mRenderInfo.textureSwitches++;

		// Set blend
		GL_CHECK(glEnable(GL_BLEND));
		GL_CHECK(glBlendFunc((int)srcBlend, (int)dstBlend));

		// Bind mesh
		auto quadMesh = ((Model*)mFullscreenQuad.get())->getMesh(0);
		quadMesh->bind(true);
		quadMesh->render();

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
		Program* p = (Program*)(mResourceMgr->getResource("__mpp_p2d_fullscreen__").get());

		setUsedProgram(p);
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
		quadMesh->render();

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

		y = mWindowHeight - y - 16;
		
		int offset = 0;
		int count = buildTextVertexBuffer(vertexBuffer, text, offset, x, y);
		vertexBuffer->mapBufferData(count);
		
		UniformCollection uc;
		uc.setUniform("COLOUR", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));
		
		Model* model = (Model*)mTextMesh.get();
		renderModelImmediate(*model, true, &uc, count);
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

		y = mWindowHeight - y - 16; 
		int count = 0, offset = 0;
		for (uint32 i = 0; i < text.size(); ++i)
		{
			count += buildTextVertexBuffer(vertexBuffer, text[i], offset, x, y - i * 16);
		}

		vertexBuffer->mapBufferData(count);

		UniformCollection uc;
		uc.setUniform("COLOUR", glm::vec4(colour.red, colour.green, colour.blue, colour.alpha));

		Model* model = (Model*)mTextMesh.get();
		renderModelImmediate(*model, true, &uc, count);
	}

	/*
	 * Render coloured text using tags.
	 *
	 */
	void RenderSystem::renderTextFormatted(std::string const& text, int x, int y)
	{
		Model* textModel = (Model*)mColouredTextMesh.get();
		Mesh* textMesh = textModel->getMesh(0);
		VertexBuffer* vertexBuffer = textMesh->getVertexBuffer(0);

		y = mWindowHeight - y - 16;

		int offset = 0;
		int count = buildColouredTextVertexBuffer(vertexBuffer, text, offset, x, y);
		vertexBuffer->mapBufferData(count);

		UniformCollection uc;
		uc.setUniform("COLOUR", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

		renderModelImmediate(*textModel, true, &uc, count);
	}
	
	/*
	 * Render debugging panel.
	 *
	 */
	void RenderSystem::renderDebugPanel()
	{
		setProjection2dOrthographic();

		vector<string> lines;

#ifdef MPP_PROFILE_BUILD
		map<string, uint64> profileResults = mProfiler->getSamples();

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
			uint64 result = it->second;

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

				msg = utils::StringUtils::format("{}: {} {}", profile, result, unit);
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

				msg = utils::StringUtils::format("{}: {} {}", profile, result, unit);
			}
			else if (profile == "Total GPU memory" ||
				profile == "Total available GPU memory" ||
				profile == "Current available GPU memory")
			{
				msg = utils::StringUtils::format("{}: {} Kb", profile, result);
			}
			else
			{
				msg = utils::StringUtils::format("{}: {}", profile, result);
			}

			lines.push_back(msg);
		}
#else
		lines.push_back(utils::StringUtils::format("Batches: {}", mRenderInfo.batchCount));
		lines.push_back(utils::StringUtils::format("Primitives: {}", mRenderInfo.primitivesRendered));
#endif

		lines.push_back(utils::StringUtils::format("Program switches: {}", mRenderInfo.programSwitches));
		lines.push_back(utils::StringUtils::format("Texture switches: {}", mRenderInfo.textureSwitches));
		lines.push_back(utils::StringUtils::format("Screen quads: {}", mRenderInfo.fullscreenQuads));

		int width = 0;
		for (auto const& line: lines)
		{
			width = max(width, (int)line.length());
		}

		// Background, in case the screen in that location is the same colour as the text
		int w = width * 8, h = (lines.size() + 1) * 16 - 8;
		int x = 0, y = mWindowHeight - h;
		
		renderQuad(x, mWindowHeight, w, h, Colour(0.5f, 0.625f, 0.87f, 0.85f), true, false);

		// Render batch information
		renderText(lines, x, y, mpp::Colour::Yellow);

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
		for (uint32 i = 0; i < mProfileTimeSamples.size(); ++i)
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

		mProfileLines->finishUpdate(mProfileTimeSamples.size() * 5 + 2, false);

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

		for (auto modelInstance: mModelInstances)
		{
			auto const& modelMeshInstances = modelInstance->getMeshInstances();
			for (auto const& mi: modelMeshInstances)
			{
				if (mi->render())
				{
					// Create sort key
					uint64 sortKey = 0;

					auto material = mi->mwMaterial ? mi->mwMaterial : (Material*)(mi->mwMesh->getMaterial().get());
					int numTextures = material->getNumTextures();

					// Texture 0.
					uint64 texture0Key = (uint64)(numTextures > 0 ? ((Texture*)material->getTexture(0).get())->getSortId() : 0);
					texture0Key <<= MPP_RENDER_SORT_TEXTURE0_BITS_OFFSET;
					
					sortKey |= texture0Key;

					// Texture 1
					uint64 texture1Key = (uint64)(numTextures > 1 ? ((Texture*)material->getTexture(1).get())->getSortId() : 0);
					texture1Key <<= MPP_RENDER_SORT_TEXTURE1_BITS_OFFSET;
					
					sortKey |= texture1Key;

					// Program
					uint64 programKey = (uint64)((Program*)material->getProgram().get())->getSortId();
					programKey <<= MPP_RENDER_SORT_PROGRAM_BITS_OFFSET;
					sortKey |= programKey;

					meshInstances.push_back(make_pair(sortKey, mi));

					// Depth
					// modelMatrix is used to create final transform, so can use this
					auto modelMatrix = m3dModelMatrix * mi->mLocalTransform;
					
					auto cameraPos = glm::vec3(m3dCameraInverseMatrix[3]);
					auto modelPos = glm::vec3(modelMatrix[3]);

					float distanceToModel = glm::distance(cameraPos, modelPos);
					float distanceInScene = min(distanceToModel / mFarPlaneDistance, 1.0f);

					uint64 distanceKey = 1 << MPP_RENDER_SORT_DEPTH_BITS_SIZE;
					distanceKey = (uint64)(distanceKey * distanceInScene);

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
		uint64 currentProgramKey = 0; // Sort ids start at 1, so this is guaranteed not to be one.
		uint64 currentTexture0Key = 0, currentTexture1Key = 0;

		for (auto meshInstance: meshInstances)
		{
			// Mask off program and see if it has changed from previous.
			uint64 thisProgramKey = meshInstance.first;
			thisProgramKey >>= MPP_RENDER_SORT_PROGRAM_BITS_OFFSET;
			thisProgramKey &= ((1 << MPP_RENDER_SORT_PROGRAM_BITS_SIZE) - 1);

			// Set program
			bool programChanged = false;
			if (thisProgramKey != currentProgramKey)
			{
				auto program = mResourceMgr->getProgramBySortId((uint32)thisProgramKey);
				setUsedProgram(program);

				currentProgramKey = thisProgramKey;
				programChanged = true;
				mRenderInfo.programSwitches++;
			}

			// Set uniforms.
			auto material = meshInstance.second->mwMaterial
				? meshInstance.second->mwMaterial
				: (Material*)(meshInstance.second->mwMesh->getMaterial().get());

			material->setUniforms();

			// Mask off texture and see if it has changed from previous.  This assumes the mesh
			// is only using one texture.
			// Mask off program and see if it has changed from previous.
			uint64 thisTexture0Key = meshInstance.first;
			thisTexture0Key >>= MPP_RENDER_SORT_TEXTURE0_BITS_OFFSET;
			thisTexture0Key &= ((1 << MPP_RENDER_SORT_TEXTURE0_BITS_SIZE) - 1);

			if (thisTexture0Key > 0 && (thisTexture0Key != currentTexture0Key || programChanged))
			{
				auto texture = mResourceMgr->getTextureBySortId((uint32)thisTexture0Key);
				texture->bind(0);

				currentTexture0Key = thisTexture0Key;
				mRenderInfo.textureSwitches++;
			}

			uint64 thisTexture1Key = meshInstance.first;
			thisTexture1Key >>= MPP_RENDER_SORT_TEXTURE1_BITS_OFFSET;
			thisTexture1Key &= ((1 << MPP_RENDER_SORT_TEXTURE1_BITS_SIZE) - 1);

			if (thisTexture1Key > 0 && (thisTexture1Key != currentTexture1Key || programChanged))
			{
				auto texture = mResourceMgr->getTextureBySortId((uint32)thisTexture1Key);
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
			if (meshInstance.second->mPrimitivesToRender == (uint32)-1)
			{
				meshInstance.second->mwMesh->render();
				mRenderInfo.primitivesRendered += meshInstance.second->mwMesh->getNumPrimitives();
			}
			else
			{
				meshInstance.second->mwMesh->render(meshInstance.second->mPrimitivesToRender);
				mRenderInfo.primitivesRendered += meshInstance.second->mPrimitivesToRender;
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

		// Delete all ModelInstances now that they're rendered.
		for (auto modelInstance: mModelInstances)
		{
			delete modelInstance;
		}

		mModelInstances.clear();
	}
	
	/*
	 * Get rendering information about this frame.
	 *
	 */
	RenderInfo const& RenderSystem::getRenderInfo() const
	{
		return mRenderInfo;
	}

	/*
	 * Start rendering.  Must be called before any other rendersystem
	 * functions in the render loop.
	 *
	 */
	void RenderSystem::startScene()
	{
		mRenderInfo.batchCount = 0;
		mRenderInfo.programSwitches = 0;
		mRenderInfo.textureSwitches = 0;
		mRenderInfo.primitivesRendered = 0;
		mRenderInfo.fullscreenQuads = 0;

		mPostProcessEffects.clear();

		resetTransform();

		setRenderTarget(mSceneTarget);
	}

	/*
	 * Finish rendering.  Must be called before swapping screen.
	 *
	 */
	RenderInfo const& RenderSystem::finishScene()
	{
		flushVertexBuffers();

		// Change to 2d to render everything out.
		setProjection2dOrthographic();
		resetTransform();

		renderToScreen();
		renderFullscreenQuad((mpp::RenderTexture*)mSceneTarget.get(), 0, mpp::BlendMode::One, mpp::BlendMode::Zero);

		// Post process
		for (auto& effect: mPostProcessEffects)
		{
			// Set render target
			setRenderTarget(mFullscreenFxTarget);
			clearScreen(mpp::Colour::Black);

			// Render effect
			renderFullscreenQuad(mResourceMgr->getResource(effect.material), &effect.uniforms);

			// Set render to screen and blend effect over it.
			renderToScreen();
			
			auto fxTexture = (mpp::RenderTexture*)mFullscreenFxTarget.get();
			renderFullscreenQuad(fxTexture, effect.attachment, effect.blendSrc, effect.blendDst);
		}

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

#ifdef MPP_DEBUG_BUILD
	/*
	 * Add an error to our report.
	 *
	 */
	void RenderSystem::addOpenGLError(string const& error, OpenGLError::Severity severity)
	{
		if (mOpenGLErrors.find(error) == mOpenGLErrors.end())
		{
			OpenGLError err;
			err.message = error;
			err.severity = severity;
			err.count = 1;
			mOpenGLErrors[error] = err;
		}
		else
		{
			OpenGLError &err = mOpenGLErrors[error];
			err.count++;
		}
	}

	/*
	 * Get a stack trace.  Used to find where an bad GL command was issued from.
	 *
	 */
	void RenderSystem::debugStackTrace()
	{
		//mStackWalker->ShowCallstack();
	}
#endif
} 