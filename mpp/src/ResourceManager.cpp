#include <cassert>

#include "mpp/ResourceManager.h"
#include "mpp/DefaultShaders.h"
#include "mpp/InternalFont.h"
#include "mpp/Program.h"
#include "mpp/StringProgramStream.h"
#include "mpp/TextureStream.h"
#include "mpp/ProgrammaticModelStream.h"
#include "mpp/ProgrammaticMaterialStream.h"
#include "mpp/MppException.h"

using namespace std;

namespace mpp
{

	uint32 ResourceManager::msSortableTextureId = 1;
	uint32 ResourceManager::msSortableProgramId = 1;

	/*
	 * Constructor.
	 *
	 */
	ResourceManager::ResourceManager(RenderSystem* renderSystem)
		: mwRenderSystem(renderSystem)
	{
		// Pad sortable vectors
		for (uint32 i = 0; i < msSortableTextureId; ++i)
		{
			mSortableTextures.push_back(nullptr);
		}

		for (uint32 i = 0; i < msSortableProgramId; ++i)
		{
			mSortablePrograms.push_back(nullptr);
		}

		//
		// Create built-in resources
		//

		// Basic 3d shaders
		set<string> attribs = { "Normal", "Texture", "TexCoords2", "RGBA" };
		auto ps = new StringProgramStream(generateShader(VertexShader3dTemplate, attribs), generateShader(FragmentShader3dTemplate, attribs));
		createResource<Program>("__mpp_p3d_tris_p3n3t2c4", ResourceStreamPtr(ps))->load();

		// 2d fullscreen program
		attribs = { "Diffuse" };
		ps = new StringProgramStream(generateShader(VertexShaderFullscreenTemplate, attribs), generateShader(FragmentShaderFullscreenTemplate, attribs));
		createResource<Program>("__mpp_p2d_fullscreen", ResourceStreamPtr(ps))->load();

		// Internal text programs
		attribs = { "Diffuse", "Points" };
		ps = new StringProgramStream(generateShader(VertexShaderTextTemplate, attribs), generateShader(FragmentShaderTextTemplate, attribs));
		createResource<Program>("__mpp_p2d_points_text", ResourceStreamPtr(ps))->load();

		attribs = { "Diffuse", "Points", "RGBA" };
		ps = new StringProgramStream(generateShader(VertexShaderTextTemplate, attribs), generateShader(FragmentShaderTextTemplate, attribs));
		createResource<Program>("__mpp_p2d_points_text_coloured", ResourceStreamPtr(ps))->load();

		attribs = { "Diffuse" };
		ps = new StringProgramStream(generateShader(VertexShaderTextTemplate, attribs), generateShader(FragmentShaderTextTemplate, attribs));
		createResource<Program>("__mpp_p2d_tris_text", ResourceStreamPtr(ps))->load();

		attribs = { "Diffuse", "RGBA" };
		ps = new StringProgramStream(generateShader(VertexShaderTextTemplate, attribs), generateShader(FragmentShaderTextTemplate, attribs));
		createResource<Program>("__mpp_p2d_tris_text_coloured", ResourceStreamPtr(ps))->load();

		// Default texture
		vector<uint8> whiteData(16, 255);

		TextureStream* blankStream = new TextureStream(&(whiteData[0]), 2, 2, 32, false);
		createResource<Texture>("__mpp_tex_none", ResourceStreamPtr(blankStream))->load();

		// Internal font texture
		InternalFont internalFont;

		TextureStream* ts = new TextureStream(
			(uint8 const*)internalFont.getData(),
			internalFont.getWidth(),
			internalFont.getHeight(),
			32,
			false);

		createResource<Texture>("__mpp_tex_internalfont", ResourceStreamPtr(ts))->load();

		// 2D materials
		// ...

		// 3D materials
		// ...
	}

	/*
	 * Destructor.
	 *
	 */
	ResourceManager::~ResourceManager()
	{
		destroyAllResources();
	}

	/*
	 * Create all resources.
	 *
	 */
	void ResourceManager::createAllResources()
	{
		for (auto it: mResources)
		{
			it.second->create();
		}
	}

	/*
	 * Destroy all resources.
	 *
	 */
	void ResourceManager::destroyAllResources()
	{
		for (auto it: mResources)
		{
			it.second->destroy();
		}
	}

	/*
	 * Load all resources.
	 *
	 */
	void ResourceManager::loadAllResources()
	{
		for (auto it: mResources)
		{
			it.second->load();
		}
	}

	/*
	 * Unload all resources.
	 *
	 */
	void ResourceManager::unloadAllResources()
	{
		for (auto it: mResources)
		{
			it.second->unload();
		}
	}

	/*
	 * Get named resource.
	 *
	 */
	ResourcePtr ResourceManager::getResource(string const& name, bool nullIfNotFound)
	{
		if (mResources.find(name) == mResources.end())
		{
			if (nullIfNotFound)
			{
				return ResourcePtr();
			}
			else
			{
				THROW_MPP(utils::StringUtils::format("Resource '{}' not found.", name), __LINE__, __FILE__, __FUNCTION__);
			}
		}

		return mResources[name];
	}

	/*
	 * Gets (or creates if not existing) a 2d program based on the given spec and flags.
	 *
	 */
	ResourcePtr ResourceManager::getOrCreateDefault2dProgram(mesh::MeshSpecification const& spec, uint32 flags, bool load)
	{
		uint32 specHash = spec.getHashCode();
		specHash |= flags;

		// Does it already exist?
		auto createdProgram = mDefaultPrograms.find(specHash);
		if (createdProgram != mDefaultPrograms.end())
		{
			return createdProgram->second;
		}

		// Get spec name and tags
		set<string> tags;
		string specName = "__mpp_p2d_";

		auto primType = spec.getPrimitiveType();
		switch (primType)
		{
		case mesh::Primitive::Type::Points:
			specName += "points_";
			tags.insert("Points");
			break;
		case mesh::Primitive::Type::Lines:
			specName += "lines_";
			break;
		case mesh::Primitive::Type::Triangles:
			specName += "tris_";
			break;
		default:
			break;
		}

		for (int i = 0; i < spec.getNumVertexBufferAttributeLayouts(); ++i)
		{
			auto const& layout = spec.getVertexBufferAttributeLayout(i);
			for (int j = 0; j < layout.getNumAttributes(); ++j)
			{
				auto const& attrib = layout.getAttribute(j);

				switch (attrib.component)
				{
				case mesh::Vertex::Component::Position2:
					specName += "p2";
					tags.insert("Position2");
					break;
				case mesh::Vertex::Component::Position3:
					specName += "p3";
					tags.insert("Position3");
					break;
				case mesh::Vertex::Component::Position4:
					specName += "p4";
					tags.insert("Position4");
					break;
				case mesh::Vertex::Component::Normal3:
					specName += "n3";
					tags.insert("Normal");
					break;
				case mesh::Vertex::Component::TexCoord2:
					specName += "t2";
					tags.insert("Texture");
					tags.insert("TexCoords2");
					break;
				case mesh::Vertex::Component::TexCoord3:
					specName += "t3";
					tags.insert("Texture");
					tags.insert("TexCoords3");
					break;
				case mesh::Vertex::Component::TexCoord4:
					specName += "t4";
					tags.insert("Texture");
					tags.insert("TexCoords4");
					break;
				case mesh::Vertex::Component::Colour1:
					specName += "c1";
					tags.insert("Alpha");
					break;
				case mesh::Vertex::Component::Colour3:
					specName += "c3";
					tags.insert("RGB");
					break;
				case mesh::Vertex::Component::Colour4:
					specName += "c4";
					tags.insert("RGBA");
					break;
				default:
					break;
				}
			}
		}

		// Give each flag a character
		if (flags != 0)
		{
			specName += "_";

			if (flags & MPP_PROGRAM_TAGS_DIFFUSE)
			{
				specName += "d";
				tags.insert("Diffuse");
			}
			if (flags & MPP_PROGRAM_TAGS_ROTATION)
			{
				specName += "r";
				tags.insert("Rotation");
			}
		}

		// Generate shaders
		auto ps = new StringProgramStream(generateShader(VertexShader2dTemplate, tags), generateShader(FragmentShader2dTemplate, tags));
		auto res = createResource<Program>(specName, ResourceStreamPtr(ps));

		if (load)
		{
			res->load();
		}

		mDefaultPrograms[specHash] = res;
		return res;
	}

	/*
	 * Get raw texture resource from sort id.
	 *
	 */
	Texture* ResourceManager::getTextureBySortId(uint32 id)
	{
		return mSortableTextures[id];
	}

	/*
	 * Get raw program resource from sort id.
	 *
	 */
	Program* ResourceManager::getProgramBySortId(uint32 id)
	{
		return mSortablePrograms[id];
	}

	/*
	 * Determine whether or not to write a shader line based on this fragment.
	 *
	 */
	bool ResourceManager::evaluateShaderDirective(string const& expression, set<string> const& attribs)
	{
		bool invert = false;
		string trimmedLine = trim_copy(expression);

		if (trimmedLine[0] == '!')
		{
			trimmedLine = trim_copy(trimmedLine.substr(1));
			if (trimmedLine == "")
			{
				THROW_MPP("Cannot have ! without an attribute in shader template.", __LINE__, __FILE__, __FUNCTION__);
			}

			invert = true;
		}

		return (attribs.find(trimmedLine) != attribs.end()) != invert;
	}

	/*
	 * Process a line of a shader template.
	 *
	 */
	bool ResourceManager::processShaderLine(string const& lineFragment, set<string> const& attribs, bool prev)
	{
		if (lineFragment == "Else")
		{
			return !prev;
		}

		uint32 orPos = lineFragment.find_first_of('|');
		uint32 andPos = lineFragment.find_first_of('&');

		if (orPos == -1 && andPos == -1)
		{
			return evaluateShaderDirective(lineFragment, attribs);
		}
		else if (orPos < andPos)
		{
			bool left = evaluateShaderDirective(lineFragment.substr(0, orPos), attribs);
			bool right = processShaderLine(lineFragment.substr(orPos + 1), attribs, prev);
			return left || right;
		}
		else if (andPos < orPos)
		{
			bool left = evaluateShaderDirective(lineFragment.substr(0, andPos), attribs);
			bool right = processShaderLine(lineFragment.substr(andPos + 1), attribs, prev);
			return left && right;
		}
		else
		{
			THROW_MPP("Could not evaluate ## directive in shader template.", __LINE__, __FILE__, __FUNCTION__);
		}
	}

	/*
	 * Generate shader for given attributes.
	 *
	 */
	string ResourceManager::generateShader(string const& templ, set<string> const& attribs)
	{
		stringstream ss(templ);
		string line, output;

		bool writeLine = true;
		while (getline(ss, line, '\n'))
		{
			string trimmedLine = trim_copy(line);
			if (trimmedLine.size() < 2 && writeLine)
			{
				output += line + "\n";
			}
			else if (trimmedLine[0] == '#' && trimmedLine[1] == '#')
			{
				trimmedLine = trim_copy(trimmedLine.substr(2));
				writeLine = trimmedLine != "" ? processShaderLine(trimmedLine, attribs, writeLine) : true;
			}
			else if (writeLine)
			{ 
				output += line + "\n";
			}
		}

		return output;
	}

}