#pragma once

#include <vector>
#include <string>
#include <map>

struct MaterialDefinition
{
public:

	enum class PositionType
	{
		p2D,
		p3D
	};

	struct Shader
	{
		enum class Type
		{
			Vertex,
			Geometry,
			Fragment
		};

		Type type;
		std::string name;
	};

	struct Texture
	{
		bool isResource;
		std::string binding;
		std::string resource;
	};

public:

	std::string name;
	
	PositionType positionType;

	std::vector<Shader> shaders;

	std::vector<Texture> textures;
};