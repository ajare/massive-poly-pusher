#include <iostream>
#include <fstream>
#include <string>

#include "mpp/mesh/MeshSpecification.h"
#include "mpp/mesh/MppMeshException.h"
#include "mpp/program/Parser.h"
#include "mpp/program/MppProgramException.h"

using namespace std;

struct ProgramArgs
{
	string vertexSource, geometrySource, fragmentSource;
};

/*
 * Print out help.
 *
 */
void printHelp()
{
	cout << "Syntax: program-builder -v <vertex-source> [-g <geometry-source>] -f <fragment-source>\n\n";
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
		else
		{
			string errMsg = "Unknown argument: " + arg;
			throw exception(errMsg.c_str());
		}
	}

	if (pArgs.vertexSource == "")
	{
		throw exception("No vertex source (-v/--vertex) specified.");
	}
	if (pArgs.fragmentSource == "")
	{
		throw exception("No fragment source (-f/--fragment) specified.");
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
		mpp::program::Parser parser;
		
		parser.setVertexSource(vsContent);
		parser.setGeometrySource(gsContent);
		parser.setFragmentSource(fsContent);

		parser.build();
	}
	catch (mpp::program::MppProgramException const& e)
	{
		cout << e.what() << "\n";
		cout << " - thrown by " + e.getFunction() << "\n";
		cout << " - thrown at " + e.getFile() + ":" + to_string(e.getLine()) << "\n";
		return 1;
	}
	catch (mpp::mesh::MppMeshException const& e)
	{
		cout << e.what() << "\n";
		cout << " - thrown by " + e.getFunction() << "\n";
		cout << " - thrown at " + e.getFile() + ":" + to_string(e.getLine()) << "\n";
		return 1;
	}
	catch (exception const& e)
	{
		cout << e.what() << "\n";
		return 1;
	}

	return 0;
}
