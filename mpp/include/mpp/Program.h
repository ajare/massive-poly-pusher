#pragma once

#pragma warning(push)
#pragma warning(disable : 4201)
#include <glm/gtc/matrix_transform.hpp>
#pragma warning(pop)

#include <map>
#include <vector>

#include "mpp/program/Parser.h"

#include "mpp/Resource.h"

#define MPP_PROGRAM_VS_IN_PREFIX			"_mpp_vs_in_"
#define MPP_PROGRAM_VS_OUT_PREFIX			"_mpp_vs_out_"

#define MPP_PROGRAM_GS_IN_PREFIX			"_mpp_gs_in_"
#define MPP_PROGRAM_GS_OUT_PREFIX			"_mpp_gs_out_"

#define MPP_PROGRAM_FS_IN_PREFIX			"_mpp_fs_in_"
#define MPP_PROGRAM_FS_OUT_PREFIX			"_mpp_fs_out_"

#define MPP_PROGRAM_MCPMATRIX_TOKEN			"@MCPMatrix"
#define MPP_PROGRAM_NORMALMATRIX_TOKEN		"@NormalMatrix"
#define MPP_PROGRAM_HALFWINDOWSIZE_TOKEN	"@HalfWindowSize"

#define MPP_PROGRAM_UNIFORM_PREFIX			"_mpp_u_"
#define MPP_PROGRAM_TEXTURE_PREFIX			"_mpp_t_"

#define MPP_PROGRAM_MCPMATRIX_NAME			(MPP_PROGRAM_UNIFORM_PREFIX "modelCameraProjection_")
#define MPP_PROGRAM_NORMALMATRIX_NAME		(MPP_PROGRAM_UNIFORM_PREFIX "normal_")
#define MPP_PROGRAM_HALFWINDOWSIZE_NAME		(MPP_PROGRAM_UNIFORM_PREFIX "halfWindowSize_")

#define MPP_PROGRAM_MARKUP_UNIFORM(token)	(MPP_PROGRAM_UNIFORM_PREFIX + token + "_")
#define MPP_PROGRAM_MARKUP_TEXTURE(token)	(MPP_PROGRAM_TEXTURE_PREFIX + token + "_")

// These must start from above MeshSpecification's maximum hash value
#define MPP_PROGRAM_TAGS_DIFFUSE			0x0800
#define MPP_PROGRAM_TAGS_ROTATION			0x1000

// Loader flags
#define MPP_PROGRAM_LOADER_NEWSTYLE			0x0001

namespace mpp
{
	class _MPPAPI Program : public Resource
	{
	public:

		struct VariableInfo
		{
			std::string def;
			std::string name;
			std::string type;
			int numComponents;
			int streamOffset;
		};

		struct TextureInfo
		{
			std::string samplerName;
			std::string markedUpName;
			int32 uniformId;
		};

	private:

		enum class ShaderType
		{
			Vertex,
			Geometry,
			Fragment
		};

	private:

		std::string mVertexSource, mFragmentSource;

		uint32 mVertexShaderId, mFragmentShaderId;

		std::map<std::string, int> mUniformIds;

		int mMcpMatrixId, mNormalMatrixId, mHalfWindowSizeId;

		std::vector<TextureInfo> mTextures;

		std::vector<VariableInfo> mVertexAttributes;

		uint32 mSortId;

		uint32_t mFlags{ 0 };

	private:

		void compileShader(uint32* id, std::string const& source, std::string const& sourceType);

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

		VariableInfo getVariableInfo(std::string const& def, std::string const& name, std::string const& type, ShaderType shaderType);

		std::vector<std::string> splitSourceIntoLines(std::string const& src);

		std::string stripComments(std::string const& src);

		std::string parseSource(std::string const& src, ShaderType shaderType, bool usingGeometryShader);

	public:

		Program(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		bool operator==(Program const& other);

		int getUniformId(std::string const& name) const;

		int getModelCameraProjectionMatrixId() const;

		int getNormalMatrixId() const;

		int getHalfWindowSizeId() const;

		void setSortId(uint32 sortId);

		uint32 getSortId() const;

		int getNumSamplers() const;

		std::string const& getSamplerName(int index) const;

		std::vector<VariableInfo> const& getVertexAttributes() const;

		void bind();
	};

}