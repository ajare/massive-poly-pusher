#include "utils/FileSystem.h"

#include "mpp/resource-parsers/FileTextureStream.h"
#include "mpp/resource-parsers/MppResourceParsersException.h"

namespace mpp
{
	namespace resource_parsers
	{

		using namespace std;

		FileTextureStream::FileTextureStream(ResourceManager* resourceMgr, string const& filepath)
			: TextureStream(resourceMgr)
			, mFilepath(filepath)
		{
		}
		
		void FileTextureStream::loadImpl()
		{
			// Get file type from extension
			auto fi = utils::FileSystem::getFile(mFilepath);
			auto ext = fi.getExtension();

			auto ser = getSerializer(ext);

			ser->loadFromFile(mFilepath);
			auto const& data = ser->getData();

			// Parse data.  Root element should be 'Texture'
			auto rootName = data.getName();

			if (rootName != "Texture")
			{
				string errMsg = "Error loading '" + mFilepath + "'.  Root element is not 'Texture'.";
				THROW_MPP_RESOURCE_PARSERS_IO(errMsg, __LINE__, __FILE__, __func__);
			}

			// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexParameter.xhtml
			for (auto it = data.begin(); it != data.end(); ++it)
			{
				auto const& entry = *it;
				if (entry.first == "filename")
				{
					// If filename is specified, then load from disk
				}
				else if (entry.first == "type")
				{
					// 1D, 2D, 3D, cubemap, etc
				}
				else if (entry.first == "filter")
				{
					// Whether to use bilinear, trilinear, anisotropic, or no filtering
				}
				else if (entry.first == "wrap")
				{
					// Type of wrapping to use 
				}
				else if (entry.first == "internalFormat")
				{
					// OpenGL internal format
				}
				else if (entry.first == "pixelFormat")
				{
					// Format to store data in
				}
				else
				{
					string errMsg = "Error loading '" + mFilepath + "'.  Unknown element '" + entry.first + "'.";
					THROW_MPP_RESOURCE_PARSERS_IO(errMsg, __LINE__, __FILE__, __func__);
				}
			}

			// Load the texture if 'filename' is specified
		}
	}
}