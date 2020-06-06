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

#include <iostream>
#include <algorithm>
#include <functional>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/Importer.hpp>

#include "utils/FileSystem.h"

#include <mpp/mesh/MeshSpecification.h>
#include <mpp/mesh/ModelSerializer.h>

#include "AssImpModelLoader.h"
#include "SpecificationParser.h"

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

	pArgs.inFile = fi.getFileName();
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
				throw exception("Cannot convert and debug model at the same time.");
			}

			pArgs.mode = "debug";
		}

		if (arg == "-o")
		{
			if (i == (argc - 1))
			{
				throw exception("Read '/o' token but found no parameter after it.");
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
				throw exception("Cannot convert and debug model at the same time.");
			}

			pArgs.mode = "convert";

			if (i == (argc - 1))
			{
				throw exception("Read '/s' token but found no parameter after it.");
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
				throw exception("Read '/m' token but found no parameter after it.");
			}
			else
			{
				pArgs.matFile = argv[++i];
			}
		}
	}

	if (pArgs.mode == "convert" && pArgs.specFile == "")
	{
		throw exception("No specification file (-s) given.");
	}

	return pArgs;
}

void convert(string const& inFile, string const& outFile, string const& specFile, string const& matFile)
{
	SpecificationParser parser(specFile);

	uint32 maxVerticesPerMesh;
	MeshSpecification meshSpec = parser.parseMeshSpecification(maxVerticesPerMesh);
	map<string, ProgramInformation> programInfo = parser.parseProgramInformation();
	map<string, MaterialInformation> materialInfo = parser.parseMaterialInformation();

	AssImpModelLoader loader(inFile, meshSpec, programInfo, materialInfo, maxVerticesPerMesh, true);

	cout << "Reading: " << inFile << endl;
	cout << "Writing: " << outFile << ", " << matFile << endl;
	cout << "Spec file: " << specFile << endl;

	loader.load();

	// Save file
	ModelSerializer fileSaver;

	int meshCount = loader.getNumMeshDefinitions();
	fileSaver.setMeshCount(meshCount);

	for (int i = 0; i < meshCount; ++i)
	{
		MeshDefinition* meshDef = loader.getMeshDefinition(i);

		fileSaver.setMeshSpecification(i, meshSpec);
		fileSaver.setName(i, meshDef->getName());
		fileSaver.setMaterial(i, meshDef->getMaterial());
		fileSaver.setPrimitiveType(i, meshDef->getPrimitiveType());
		fileSaver.setPrimitiveCount(i, meshDef->getNumPrimitives());
		fileSaver.setIndexWidth(i, meshDef->getIndexWidth());
		fileSaver.setIndexData(i, meshDef->getIndexData());

		for (int j = 0; j < meshDef->getNumVertexBufferDefinitions(); ++j)
		{
			auto bufferDef = meshDef->getVertexBufferDefinition(j);
			fileSaver.addVertexStream(i, bufferDef->getVertexCount(), bufferDef->getVertexStride(), bufferDef->getData());
		}
	}

	fileSaver.save(outFile);
	loader.writeMaterials(matFile);
}

void debug(string const& inFile, string const& outFile)
{
	ModelSerializer fileLoader;

	fileLoader.load(inFile);

	int numMeshes = fileLoader.getMeshCount();
	for (int i = 0; i < numMeshes; ++i)
	{
		string const& meshName = fileLoader.getName(i);

		int numPrimitives = fileLoader.getPrimitiveCount(i);
		auto primitiveType = fileLoader.getPrimitiveType(i);

		cout << "Mesh '" << meshName << "'." << endl << endl;

		int primitiveSize = mpp::mesh::Primitive::size(primitiveType);
		switch (primitiveType)
		{
			case mpp::mesh::Primitive::Type::Points:
				cout << numPrimitives << " points." << endl;
				break;

			case mpp::mesh::Primitive::Type::Lines:
				cout << numPrimitives << " lines." << endl;
				break;

			case mpp::mesh::Primitive::Type::Triangles:
				cout << numPrimitives << " triangles." << endl;
				break;

			default:
				cout << numPrimitives << " items of unknown primitive type." << endl;
				break;
		}

		cout << endl;

		// Get model specification
		auto const& spec = fileLoader.getMeshSpecification(i);

		// Get channels
		int numLayouts = spec.getNumVertexBufferAttributeLayouts();
		for (int j = 0; j < numLayouts; ++j)
		{
			cout << "Layout " << j << endl;
			auto const& layout = spec.getVertexBufferAttributeLayout(j);

			cout << "Id\tComponent\tType\tNormalised\tSize (bytes)" << endl;

			int numAttribs = layout.getNumAttributes();
			for (int k = 0; k < numAttribs; ++k)
			{
				auto const& attrib = layout.getAttribute(k);

				cout << attrib.attributeId << "\t" 
					 << mpp::mesh::Vertex::getComponentName(attrib.component) << "\t"
					 << mpp::mesh::Vertex::getDataTypeName(attrib.dataType) << "\t" 
					 << (attrib.normalised ? "yes" : "no") << "\t\t" 
					 << attrib.sizeInBytes() << endl;
			}

			cout << endl;

			// Get data and parse according to attrib list
			int vertexCount, vertexStride;
			shared_ptr<const int8> vertexData;
			fileLoader.getVertexStream(i, j, &vertexCount, &vertexStride, &vertexData);

			int8 const* v = vertexData.get();
			for (int k = 0; k < vertexCount; ++k)
			{
				for (int l = 0; l < numAttribs; ++l)
				{
					auto const& attrib = layout.getAttribute(l);

					// Print component
					int componentCount = mpp::mesh::Vertex::getComponentSize(attrib.component);
					for (int m = 0; m < componentCount; ++m)
					{
						switch (attrib.dataType)
						{
						case mpp::mesh::Vertex::DataType::Byte:
							cout << (int)*(int8*)v; break;
						case mpp::mesh::Vertex::DataType::UnsignedByte:
							cout << (int)*(uint8*)v; break;
						case mpp::mesh::Vertex::DataType::Short:
							cout << *(int16*)v; break;
						case mpp::mesh::Vertex::DataType::UnsignedShort:
							cout << *(uint16*)v; break;
						case mpp::mesh::Vertex::DataType::Int:
							cout << *(int32*)v; break;
						case mpp::mesh::Vertex::DataType::UnsignedInt:
							cout << *(uint32*)v; break;
						case mpp::mesh::Vertex::DataType::Float:
							cout << *(float*)v; break;
						case mpp::mesh::Vertex::DataType::Double:
							cout << *(double*)v; break;
						case mpp::mesh::Vertex::DataType::Pad1:
							cout << "Pad1"; break;
						case mpp::mesh::Vertex::DataType::Pad2:
							cout << "Pad2"; break;
						case mpp::mesh::Vertex::DataType::Pad3:
							cout << "Pad3"; break;
						case mpp::mesh::Vertex::DataType::Pad4:
							cout << "Pad4"; break;
						case mpp::mesh::Vertex::DataType::Int_2_10_10_10_REV:
							cout << *(int32*)v; break;
						case mpp::mesh::Vertex::DataType::UnsignedInt_2_10_10_10_REV:
							cout << *(uint32*)v; break;
						default:
							cout << "Unknown"; break;
						}

						cout << "\t";
						v += mpp::mesh::Vertex::getDataTypeSize(attrib.dataType);
					}
				}

				cout << endl;
			}

			cout << endl;

			// Get index data
			int indexWidth = fileLoader.getIndexWidth(i);
			int indexWidthBytes = indexWidth / 8;
			shared_ptr<const uint8> indexData = fileLoader.getIndexData(i);

			cout << "Index width: " << indexWidth << endl;

			uint8 const* indexDataPtr = indexData.get();
			for (int j = 0; j < numPrimitives; ++j)
			{
				for (int k = 0; k < primitiveSize; ++k)
				{
					if (indexWidth == 16)
					{
						cout << (uint16)*(uint16 const*)indexDataPtr;
					}
					else
					{
						cout << (uint32)*(uint32 const*)indexDataPtr;
					}

					cout << "\t";
					indexDataPtr += indexWidthBytes;
				}

				cout << endl;
			}
		}

		cout << endl;
	}
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
			throw exception("Nothing to do!");
		}

	
	}
	catch (exception& e)
	{
		cout << e.what() << endl;
	}

	return 0;
}