#include "utils/FileSystem.h"

#include "mpp/DefaultShaders.h"

#include "mpp/resource-parsers/FileProgramStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, string const& filepath)
			: ProgramStream(resourceMgr)
			, mFilepath(filepath)
			, mData("") 
		{
		}

		FileProgramStream::FileProgramStream(ResourceManager* resourceMgr, utils::StructuredData const& data)
			: ProgramStream(resourceMgr)
			, mData(data)
		{
		}

		string FileProgramStream::readTextFile(string const& filepath)
		{
			string fpath{ filepath };

			// if the filepath is relative, prepend the path of the xml file
			filesystem::path fp(filepath);
			if (fp.is_relative())
			{
				filesystem::path fileFp(mFilepath);
				fpath = utils::FileSystem::concatPaths(fileFp.parent_path().string(), filepath);
			}

			// Load
			ifstream fstr(fpath);
			string str;

			fstr.seekg(0, ios::end);
			str.reserve((size_t)fstr.tellg());
			fstr.seekg(0, ios::beg);

			str.assign((istreambuf_iterator<char>(fstr)), istreambuf_iterator<char>());
			return str;
		}

		void FileProgramStream::parseQualitySetting(utils::StructuredData const& data)
		{
			string name;

			// Read settings required to create a Parser
			string vertexShader, geometryShader, fragmentShader, positionType;

			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "name")
				{
					name = entry.second.getValue();
				}
				else if (entry.first == "positionType")
				{
					positionType = value;
				}
				else if (entry.first == "vertexShader")
				{
					vertexShader = entry.second.getValue();
				}
				else if (entry.first == "geometryShader")
				{
					THROW_MPP_RESOURCE_PARSERS_NOTIMP("Geometry shaders", __LINE__, __FILE__, __func__);
					//geometryShader = entry.second.getValue();
				}
				else if (entry.first == "fragmentShader")
				{
					fragmentShader = entry.second.getValue();
				}
			}

			if (positionType != "2D" && positionType != "3D")
			{
				string errMsg = "Error loading " + mFilepath + ".  Invalid (or absent) position type specified for program.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			auto newSettingId = createQualitySetting(name);
			
			QualitySetting qs;
			qs.parser = make_shared<program::Parser>();
			
			// Load source into parser
			if (vertexShader == "")
			{
				if (positionType == "2D")
				{
					qs.parser->setVertexSource(VertexShader2dTemplate);
				}
				else if (positionType == "3D")
				{
					qs.parser->setVertexSource(VertexShader3dTemplate);
				}
			}
			else
			{
				qs.parser->setVertexSource(readTextFile(vertexShader));
			}

			if (fragmentShader == "")
			{
				if (positionType == "2D")
				{
					qs.parser->setVertexSource(FragmentShader2dTemplate);
				}
				else if (positionType == "3D")
				{
					qs.parser->setVertexSource(FragmentShader3dTemplate);
				}
			}
			else
			{
				qs.parser->setVertexSource(readTextFile(fragmentShader));
			}

			mQualitySettings[newSettingId] = qs;
		}

		void FileProgramStream::loadImpl()
		{
			// Get file type from extension
			if (mFilepath != "")
			{
				auto fi = utils::FileSystem::getFile(mFilepath);
				auto ext = fi.getExtension();

				auto ser = getSerializer(ext);

				ser->loadFromFile(mFilepath);
				mData = ser->getData();
			}

			// Parse data.  Root element should be 'Program'
			auto rootName = mData.getName();

			if (rootName != "Program")
			{
				string errMsg = "Error loading " + mFilepath + ".  Root element is not 'Program'.";
				THROW_MPP_RESOURCE_PARSERS(errMsg, __LINE__, __FILE__, __func__);
			}

			parseQualitySetting(mData);

			for (auto it = mData.begin(); it != mData.end(); ++it)
			{
				auto const& entry = *it;
				string value = utils::StringUtils::toUpper(entry.second.getValue());

				if (entry.first == "Quality")
				{
					parseQualitySetting(entry.second);
				}
			}

			ProgramStream::loadImpl();
		}
	}
}