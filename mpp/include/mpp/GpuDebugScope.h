#pragma once

#include <string>

#include <glew/glew.h>

namespace mpp
{
	// RAII GPU event scope consumed by RenderDoc, Nsight and KHR_debug tooling.
	// It is a no-op when neither KHR_debug nor EXT_debug_marker is available.
	class GpuDebugScope
	{
		enum class Backend { None, Khr, Ext };
		Backend mBackend{ Backend::None };

	public:
		explicit GpuDebugScope(std::string const& label)
		{
			if (label.empty()) return;
			if (GLEW_VERSION_4_3 || GLEW_KHR_debug)
			{
				glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label.c_str());
				mBackend = Backend::Khr;
			}
			else if (GLEW_EXT_debug_marker)
			{
				glPushGroupMarkerEXT(0, label.c_str());
				mBackend = Backend::Ext;
			}
		}

		~GpuDebugScope()
		{
			if (mBackend == Backend::Khr) glPopDebugGroup();
			else if (mBackend == Backend::Ext) glPopGroupMarkerEXT();
		}

		GpuDebugScope(GpuDebugScope const&) = delete;
		GpuDebugScope& operator=(GpuDebugScope const&) = delete;
	};

	inline void insertGpuDebugMarker(std::string const& label)
	{
		if (label.empty()) return;
		if (GLEW_VERSION_4_3 || GLEW_KHR_debug)
			glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, label.c_str());
		else if (GLEW_EXT_debug_marker)
			glInsertEventMarkerEXT(0, label.c_str());
	}
}
