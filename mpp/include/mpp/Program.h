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

#define MPP_PROGRAM_VIEWPOS_TOKEN			"@ViewPos"
#define MPP_PROGRAM_MMATRIX_TOKEN			"@MMatrix"
#define MPP_PROGRAM_MCPMATRIX_TOKEN			"@MCPMatrix"
#define MPP_PROGRAM_NORMALMATRIX_TOKEN		"@NormalMatrix"
#define MPP_PROGRAM_HALFWINDOWSIZE_TOKEN	"@HalfWindowSize"
#define MPP_PROGRAM_POINTSIZE_TOKEN			"@PointSize"

#define MPP_PROGRAM_UNIFORM_PREFIX			"_mpp_u_"
#define MPP_PROGRAM_TEXTURE_PREFIX			"_mpp_t_"

#define MPP_PROGRAM_VIEWPOS_NAME			(MPP_PROGRAM_UNIFORM_PREFIX "viewPos_")
#define MPP_PROGRAM_MMATRIX_NAME			(MPP_PROGRAM_UNIFORM_PREFIX "model_")
#define MPP_PROGRAM_MCPMATRIX_NAME			(MPP_PROGRAM_UNIFORM_PREFIX "modelCameraProjection_")
#define MPP_PROGRAM_NORMALMATRIX_NAME		(MPP_PROGRAM_UNIFORM_PREFIX "normal_")
#define MPP_PROGRAM_HALFWINDOWSIZE_NAME		(MPP_PROGRAM_UNIFORM_PREFIX "halfWindowSize_")
#define MPP_PROGRAM_POINTSIZE_NAME			(MPP_PROGRAM_UNIFORM_PREFIX "pointSize_")

#define MPP_PROGRAM_MARKUP_UNIFORM(token)	(MPP_PROGRAM_UNIFORM_PREFIX + token + "_")
#define MPP_PROGRAM_MARKUP_TEXTURE(token)	(MPP_PROGRAM_TEXTURE_PREFIX + token + "_")

// These must start from above MeshSpecification's maximum hash value
#define MPP_PROGRAM_TAGS_TEXTURE 			(1 << 10)
#define MPP_PROGRAM_TAGS_DIFFUSE			(1 << 11)
#define MPP_PROGRAM_TAGS_ROTATION			(1 << 12)
#define MPP_PROGRAM_TAGS_ATLAS				(1 << 13)
#define MPP_PROGRAM_TAGS_PRIM_POINTS		(1 << 14)
#define MPP_PROGRAM_TAGS_PRIM_LINES			(1 << 15)
#define MPP_PROGRAM_TAGS_PRIM_TRIANGLES		(1 << 16)

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
			int32_t uniformId;
		};

	private:

		enum class ShaderType
		{
			Vertex,
			Geometry,
			Fragment
		};

	private:

		mesh::MeshSpecification mMeshSpecification;

		std::string mVertexSource, mFragmentSource;

		uint32_t mVertexShaderId, mFragmentShaderId;

		std::map<std::string, int> mUniformIds;
		std::map<std::string, uint32_t> mUniformTypes;

		int mViewPosId, mMMatrixId, mMcpMatrixId, mNormalMatrixId, mHalfWindowSizeId, mPointSizeId;

		std::vector<TextureInfo> mTextures;

		std::vector<VariableInfo> mVertexAttributes;

		uint32_t mSortId;

		uint32_t mFlags{ 0 };

	private:

		void compileShader(uint32_t* id, std::string const& source, std::string const& sourceType);

		VariableInfo getVariableInfo(std::string const& def, std::string const& name, std::string const& type, ShaderType shaderType);

		std::vector<std::string> splitSourceIntoLines(std::string const& src);

		std::string stripComments(std::string const& src);

	protected:

		void createImpl();

		void destroyImpl();

		void loadImpl();

		void unloadImpl();

	public:

		Program(std::string const& name, RenderSystem* renderSystem, ResourceManager* resourceMgr, ResourceStreamPtr resourceStream);

		bool operator==(Program const& other);

		int getIdCount() const override;

		int getLiveIdCount() const override;

		mesh::MeshSpecification const& getMeshSpecification() const;

		int getUniformId(std::string const& name, int index = -1) const;
		std::vector<std::string> getUniformNames() const;
		uint32_t getUniformGlType(std::string const& name) const;
		uint32_t getSamplerGlType(std::string const& name) const;

		int getViewPosId() const;

		int getModelMatrixId() const;

		int getModelCameraProjectionMatrixId() const;

		int getNormalMatrixId() const;

		int getHalfWindowSizeId() const;

		int getPointSizeId() const;

		void setSortId(uint32_t sortId);

		uint32_t getSortId() const;

		int getNumSamplers() const;

		std::string const& getSamplerName(int index) const;

		std::vector<VariableInfo> const& getVertexAttributes() const;

		// Validates that fragment locations [0, requiredCount) are active. Extra
		// outputs are allowed so one MRT-capable shader can use single-target fallback.
		bool validateFragmentOutputLocations(size_t requiredCount, std::string& diagnostic) const;

		void bind();
	};

}