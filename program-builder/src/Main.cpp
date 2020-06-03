#include <iostream>
#include <Windows.h>

#include "mpp/mesh/MeshSpecification.h"
#include "mpp/mesh/MppMeshException.h"
#include "mpp/program/Parser.h"
#include "mpp/program/MppProgramException.h"

using namespace std;

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{

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
