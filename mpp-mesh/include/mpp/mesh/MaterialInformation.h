#pragma once

#include <string>
#include <map>
#include <vector>

#include "mpp/mesh/Config.h"

namespace mpp
{
	namespace mesh
	{
		class _MPPMESHAPI MaterialInformation
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

		private:

			std::string mName;

			PositionType mPositionType;

			std::vector<Shader> mShaders;

			std::vector<Texture> mTextures;

		public:

			MaterialInformation() {}

			explicit MaterialInformation(std::string const& name);

			std::string const& getName() const;

			void setPositionType(PositionType positionType);

			PositionType getPositionType() const;

			void addShader(Shader::Type type, std::string const& name);

			std::vector<Shader> const& getShaders() const;

			void addTexture(bool isResource, std::string const& binding, std::string const& resource);

			std::vector<Texture> const& getTextures() const;
		};
	}
}