#pragma once

#include <vector>
#include <fstream>

#include "mpp/ResourceStream.h"

#include "mpp/program/Parser.h"
#include "mpp/program/Attribute.h"

namespace mpp
{
	class _MPPAPI ProgramStream : public ResourceStream
	{
		friend class ResourceStreamSerializer;

	public:

		struct Shader
		{
			enum class Type
			{
				Default,
				File,
				Resource
			};

			Type type{ Type::Default };
			std::string source, data;
		};

	protected:

		struct QualitySetting
		{
			std::shared_ptr<program::Parser> parser;
			Shader vertexShader, geometryShader, fragmentShader;
		};

	private:

		std::string mVertexSource, mGeometrySource, mFragmentSource;

	protected:

		std::vector<QualitySetting> mQualitySettings;

		std::set<std::string> mAttribs;

	protected:

		void loadImpl();

		void setVertexSource(std::string const& src);

		void setGeometrySource(std::string const& src);

		void setFragmentSource(std::string const& src);

	public:

		explicit ProgramStream(ResourceManager* resourceMgr);

		std::string const& getVertexSource() const;

		std::string const& getGeometrySource() const;

		std::string const& getFragmentSource() const;

		Shader const& getVertexShader() const;

		Shader const& getGeometryShader() const;

		Shader const& getFragmentShader() const;

		std::string getConcatenatedSource();

		std::vector<program::Attribute> getInAttributes() const;

		std::vector<std::string> getUniforms() const;

		std::vector<std::string> getTextures() const;

		uint32_t createQualitySetting(std::string const& name);
	};
}