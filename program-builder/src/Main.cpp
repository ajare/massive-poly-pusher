#include <iostream>
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
			pArgs.vertexSource = argv[i + 1];
		}
		else if (arg == "-g" || arg == "--geometry")
		{
			pArgs.geometrySource = argv[i + 1];
		}
		else if (arg == "-f" || arg == "--fragment")
		{
			pArgs.fragmentSource = argv[i + 1];
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
		mpp::program::Parser parser;
		
		parser.setVertexSource(pArgs.vertexSource);
		parser.setGeometrySource(pArgs.geometrySource);
		parser.setFragmentSource(pArgs.fragmentSource);

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
