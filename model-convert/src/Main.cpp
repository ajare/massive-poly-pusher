/*
Specification:
	A Model specification consists of three parts : Mesh specification, program specification, and material specification.

Mesh specification:
	This specifies the layout of the VBO attributes that the mesh(es) should use.
	It is defined in the 'Buffers' element, a child of the root 'Specification' node.
		Buffers attributes:
			@indexed: 'yes' / 'true', 'no' / 'false'
				Specifies whether the primitives should use indexing or not.
			@primitive: 'points', 'lines', 'triangles'
				Specifies whether input triangles should be decomposed to points, lines, or kept as triangles.  It is inefficient to use points or lines if
				the data is not indexed.
			@storage: 'static', 'dynamic'
				Specifies whether the mesh should use static storage, or dynamic (GL_STATIC_DRAW vs GL_DYNAMIC_DRAW).
			@splitSize:
				Specifies the maximum number of vertices per mesh.  If left out, does not split.

	The 'Buffers' element has 'Buffer' children, which are used to specify (in order) different VBO definitions.
		The 'Buffer' element has 'Channel' children, which are used to specify (in order) the vertex attributes.
			Channel attributes:
				@data: 'position2', 'position3', 'position4', 'normal3', 'normal4', 'texcoord2', 'texcoord3', 'texcoord4', 'colour3', 'colour4'
					Specifies the component type.
				@type: 'float', 'ubyte', 'ushort'
					Specifies the data type.
				@normalised [optional, defaults to false]: 'yes' / 'true', 'no' / 'false'
					Specifies whether the data should be normalised.

Program specification
	This defines programs to be used by the materials.  It is expected that the programs already exist as resources, ready to be used by the materials.
	It is defined in the 'Programs' element, a child of the root 'Specification' node.
	The 'Programs' element has 'Program' children, each of which define one program.
	The 'Program' element' has a 'Textures' child.
	The 'Textures' element has 'Texture' children, which define a binding of a resource to a texture unit.
		Texture attributes:
			@binding: the name (un-marked up) of the shader sampler to bind to
			@index: if no binding given, then index specifies the zero-offset texture unit to bind to.

		The 'Texture' element has a 'Resource' child, which specifies the name of the resource to bind (this must be an image).
			Resource attributes:
				@input: the name of the resource

Materials specification
	This defines filters to generate materials, given inputs from a source.
	It is defined in the 'Materials' element, a child of the root 'Specification' node.
	It has the following child nodes:
		Name:
			Name nodes have an 'input' attribute, and a set of 'Filter' children, specified by the 'type' attribute.  Currently only 'regex' filter is implemented.
				Name attributes:
					@input: either '${material.name}' or a literal.
			
			Each filter has 'Param' children, which have name/value attributes.  For example, a filter might be:
				
				<Filter type="regex">
					<Param name="expression" value=".*_(.*)" />
				</Filter>
			
			This takes the input from the previous stage and captures everything after '_', to be passed onto the next stage.

		Program:
			Program nodes have a 'Name' child (see above).

*/

#if defined(_MSC_VER) && _MSC_VER < 1930
#  include <vld.h> // Memory tracking
#endif


#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <functional>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>

#include "utils/FileSystem.h"

#include <mpp/ResourceStreamSerializer.h>
#include <mpp/BasicMaterialStream.h>
#include <mpp/ModelSerializer.h>

#include <mpp/mesh/MeshSpecification.h>

#include <mpp/mesh-specification-parser/SpecificationParser.h>
#include <mpp/mesh-specification-parser/ProgramInformation.h>
#include <mpp/mesh-specification-parser/ModelspecStream.h>

#include "AssImpModelLoader.h"

#define MPPMODEL_FILEEXT ".mppmodel"
#define MATERIAL_FILEEXT ".material"

using namespace std;
using namespace Assimp;
using namespace mpp::mesh;

struct ProgramArgs
{
	string mode;
	string inFile, outFile, specFile, matFile;
};

/*
 * Print out help.
 *
 */
void printHelp()
{
	cout << "Syntax: model-convert <file> -s specfile [-o <outfile>]\n\n";
}

/*
 * Parse command line arguments.
 *
 */
ProgramArgs parseArguments(int argc, char** argv)
{
	ProgramArgs pArgs;

	// Set defaults.
	auto fi = utils::FileSystem::FileInfo(string(argv[1]));

	pArgs.inFile = fi.getFilePath();
	pArgs.outFile = fi.getFileNameWithoutExtension() + MPPMODEL_FILEEXT;
	pArgs.specFile = "";
	pArgs.matFile = fi.getFileNameWithoutExtension() + MATERIAL_FILEEXT;

	for (int i = 2; i < argc; ++i)
	{
		string arg = argv[i];
		
		if (arg == "-d")
		{
			if (pArgs.mode == "convert")
			{
				throw runtime_error("Cannot convert and debug model at the same time.");
			}

			pArgs.mode = "debug";
		}

		if (arg == "-o")
		{
			if (i == (argc - 1))
			{
				throw runtime_error("Read '-o' token but found no parameter after it.");
			}
			else
			{
				pArgs.outFile = argv[++i];
			}
		}
		else if (arg == "-s")
		{
			if (pArgs.mode == "debug")
			{
				throw runtime_error("Cannot convert and debug model at the same time.");
			}

			pArgs.mode = "convert";

			if (i == (argc - 1))
			{
				throw runtime_error("Read '-s' token but found no parameter after it.");
			}
			else
			{
				pArgs.specFile = argv[++i];
			}
		}
		else if (arg == "-m")
		{
			if (i == (argc - 1))
			{
				throw runtime_error("Read '-m' token but found no parameter after it.");
			}
			else
			{
				pArgs.matFile = argv[++i];
			}
		}
	}

	if (pArgs.mode == "convert" && pArgs.specFile == "")
	{
		throw runtime_error("No specification file (-s) given.");
	}

	return pArgs;
}

void serializeMaterial(mpp::ResourceStreamPtr const& matStream, mpp::mesh::MeshSpecification const& meshSpec, ofstream& fp)
{
	mpp::ResourceStreamSerializer ser(nullptr);

	ser.setGlobalMeshSpecification(meshSpec);
	ser.serialize(matStream, fp);
}

void convert(string const& inFile, string const& outFile, string const& specFile, string const& matFile)
{
	using namespace mpp::mesh_specification_parser;

	// Load spec
	ModelspecStream mStream(specFile);
	mStream.load();

	uint32_t maxVerticesPerMesh{ ~0u };
	AssImpModelLoader loader(inFile, mStream.getMeshSpecification(), maxVerticesPerMesh, true);

	cout << "Reading: " << inFile << endl;
	cout << "Writing: " << outFile << ", " << matFile << endl;
	cout << "Spec file: " << specFile << endl;

	loader.load();
	
	// Save file
	mpp::ModelSerializer fileSaver;

	// Materials
	auto const& materials = mStream.getMaterials();
	for (auto const& kvp: materials)
	{
		fileSaver.addMaterial(kvp.first, kvp.second);
	}

	int meshCount = loader.getNumMeshDefinitions();
	fileSaver.setMeshCount(meshCount);

	for (int i = 0; i < meshCount; ++i)
	{
		MeshDefinition* meshDef = loader.getMeshDefinition(i);

		fileSaver.setName(i, meshDef->getName());
		auto materialName = meshDef->getMaterial();
		if (materials.find(materialName) == materials.end())
		{
			// Some Assimp importers expose a truncated or generated material name.
			// A one-material specification is unambiguous, so map it to that
			// authored material rather than emitting an unloadable model.
			if (materials.size() != 1)
			{
				throw runtime_error("Mesh material does not match a material in the model specification.");
			}
			materialName = materials.begin()->first;
		}
		fileSaver.setMaterial(i, materialName);
		fileSaver.setPrimitiveType(i, meshDef->getPrimitiveType());
		fileSaver.setPrimitiveCount(i, meshDef->getNumPrimitives());
		fileSaver.setIndexBuffer(i, meshDef->getIndexData(), meshDef->getIndexWidth());

		for (size_t j = 0; j < meshDef->getNumVertexBufferDefinitions(); ++j)
		{
			auto bufferDef = meshDef->getVertexBufferDefinition(j);
			fileSaver.addVertexStream(i, bufferDef->getVertexCount(), bufferDef->getVertexStride(), bufferDef->getData());
		}
	}

	fileSaver.save(outFile);
}

void debug(string const& inFile, string const& outFile)
{
	mpp::ModelSerializer ser;

	ser.load(inFile);
}

/*
 * Entry point.
 *
 */
int main(int argc, char** argv)
{
	if (argc < 3)
	{
		printHelp();
		return 1;
	}

	try
	{
		ProgramArgs programArgs = parseArguments(argc, argv);

		if (programArgs.mode == "convert")
		{
			convert(programArgs.inFile, programArgs.outFile, programArgs.specFile, programArgs.matFile);
		}
		else if (programArgs.mode == "debug")
		{
			debug(programArgs.inFile, programArgs.outFile);
		}
		else
		{
			throw runtime_error("Nothing to do!");
		}
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}

	return 0;
}