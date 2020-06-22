#include <iostream>
#include <fstream>
#include <string>

#include "mpp/mesh/MeshSpecification.h"
#include "mpp/mesh/MppMeshException.h"

#include "mpp/mesh-specification-parser/SpecificationParser.h"

#include "mpp/program/Parser.h"
#include "mpp/program/MppProgramException.h"


using namespace std;

struct ProgramArgs
{
	string vertexSource, geometrySource, fragmentSource;
	string meshSpec;
	set<string> attribs;
};

/*
 * Print out help.
 *
 */
void printHelp()
{
	cout << "Syntax: program-builder -v <vertex-source> [-g <geometry-source>] -f <fragment-source> -m <mesh-spec>\n\n";
}

/*
 * Parse command line arguments.
 *
 */
ProgramArgs parseArguments(int argc, char** argv)
{
	ProgramArgs pArgs;

	for (int i = 1; i < argc; ++i)
	{
		string arg = argv[i];

		if (arg == "-v" || arg == "--vertex")
		{
			pArgs.vertexSource = argv[++i];
		}
		else if (arg == "-g" || arg == "--geometry")
		{
			pArgs.geometrySource = argv[++i];
		}
		else if (arg == "-f" || arg == "--fragment")
		{
			pArgs.fragmentSource = argv[++i];
		}
		else if (arg == "-m" || arg == "--meshspec")
		{
			pArgs.meshSpec = argv[++i];
		}
		else
		{
			pArgs.attribs.insert(argv[i]);
		}
	}

	if (pArgs.vertexSource == "")
	{
		throw exception("No vertex source (-v/--vertex) given.");
	}
	if (pArgs.fragmentSource == "")
	{
		throw exception("No fragment source (-f/--fragment) given.");
	}
	if (pArgs.meshSpec == "")
	{
		throw exception("No mesh specification (-m/--meshspec) given.");
	}

	return pArgs;
}

int main(int argc, char** argv)
{
	// Parse arguments
	if (argc < 3)
	{
		printHelp();
		return 1;
	}

	ProgramArgs pArgs = parseArguments(argc, argv);

	try
	{
		// Load in files
		ifstream vsFile, gsFile, fsFile;
		string vsContent, gsContent, fsContent;
		
		vsFile.open(pArgs.vertexSource, ifstream::in);
		if (!vsFile.is_open())
		{
			string errMsg = "Could not open " + pArgs.vertexSource;
			throw exception(errMsg.c_str());
		}

		vsContent = string((istreambuf_iterator<char>(vsFile)), (istreambuf_iterator<char>()));

		if (pArgs.geometrySource != "")
		{
			gsFile.open(pArgs.geometrySource, ifstream::in);
			if (!gsFile.is_open())
			{
				string errMsg = "Could not open " + pArgs.geometrySource;
				throw exception(errMsg.c_str());
			}

			gsContent = string((istreambuf_iterator<char>(gsFile)), (istreambuf_iterator<char>()));
		}

		fsFile.open(pArgs.fragmentSource, ifstream::in);
		if (!fsFile.is_open())
		{
			string errMsg = "Could not open " + pArgs.fragmentSource;
			throw exception(errMsg.c_str());
		}

		fsContent = string((istreambuf_iterator<char>(fsFile)), (istreambuf_iterator<char>()));

		// Parse
		mpp::mesh_specification_parser::SpecificationParser specParser(pArgs.meshSpec);
		uint32 maxVerticesPerMesh;
		
		mpp::mesh::MeshSpecification meshSpec = specParser.parseMeshSpecification(maxVerticesPerMesh);

		mpp::program::Parser sourceParser;
		
		sourceParser.setMeshSpecification(meshSpec);
		sourceParser.setVertexSource(vsContent);
		sourceParser.setGeometrySource(gsContent);
		sourceParser.setFragmentSource(fsContent);

		sourceParser.build(pArgs.attribs);

		auto const& warnings = sourceParser.getWarnings();
		for (auto const& warning: warnings)
		{
			cout << "Warning: " << warning << "\n";
		}

		auto const& errors = sourceParser.getErrors();
		for (auto const& error : errors)
		{
			cout << "Error: " << error << "\n";
		}

		if (!errors.empty())
		{
			throw exception("Errors were found during parsing.");
		}
	}
	catch (mpp::program::MppProgramException const& e)
	{
		cout << "\n" << e.what() << "\n";
		cout << " - thrown by " + e.getFunction() << "\n";
		cout << " - thrown at " + e.getFile() + ":" + to_string(e.getLine()) << "\n";
		return 1;
	}
	catch (mpp::mesh::MppMeshException const& e)
	{
		cout << "\n" << e.what() << "\n";
		cout << " - thrown by " + e.getFunction() << "\n";
		cout << " - thrown at " + e.getFile() + ":" + to_string(e.getLine()) << "\n";
		return 1;
	}
	catch (exception const& e)
	{
		cout << "\n" << e.what() << "\n";
		return 1;
	}

	return 0;
}
