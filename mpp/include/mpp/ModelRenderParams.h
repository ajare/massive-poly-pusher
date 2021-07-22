#pragma once

#include <memory>
#include <string>
#include <map>

#include "mpp/Config.h"
#include "mpp/UniformCollection.h"

namespace mpp
{
	class ModelRenderParams
	{
	public:

		static const uint32_t Flag_Visible		= 0x01;
		static const uint32_t Flag_Wireframe	= 0x02;

	public:

		struct MeshRenderParams
		{
			uint32_t flags{ Flag_Visible };
			uint32_t instanceCount{ 1 };
			std::shared_ptr<UniformCollection> uniforms;
		};

	private:

		std::map<std::string, MeshRenderParams> mMeshParams;

		MeshRenderParams mModelParams;

	public:

		ModelRenderParams()
		{
		}

		void setModelFlags(uint32_t flags)
		{
			auto it = mMeshParams.insert(std::make_pair("", MeshRenderParams())).first;
			it->second.flags = flags;
		}

		void setModelInstanceCount(uint32_t count)
		{
			auto it = mMeshParams.insert(std::make_pair("", MeshRenderParams())).first;
			it->second.instanceCount = count;
		}

		void setModelUniforms(std::shared_ptr<UniformCollection> uniforms)
		{
			auto it = mMeshParams.insert(std::make_pair("", MeshRenderParams())).first;
			it->second.uniforms = uniforms;
		}

		void setMeshFlags(std::string const& mesh, uint32_t flags)
		{
			auto it = mMeshParams.insert(std::make_pair(mesh, MeshRenderParams())).first;
			it->second.flags = flags;
		}

		void setMeshInstanceCount(std::string const& mesh, uint32_t count)
		{
			auto it = mMeshParams.insert(std::make_pair(mesh, MeshRenderParams())).first;
			it->second.instanceCount = count;
		}

		void setMeshUniforms(std::string const& mesh, std::shared_ptr<UniformCollection> uniforms)
		{
			auto it = mMeshParams.insert(std::make_pair(mesh, MeshRenderParams())).first;
			it->second.uniforms = uniforms;
		}

		std::map<std::string, MeshRenderParams> const& getMeshParams() const
		{
			return mMeshParams;
		}
	};
}

