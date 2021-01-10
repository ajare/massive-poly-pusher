#include "mpp/MaterialStream.h"
#include "mpp/ProgramStream.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	MaterialStream::MaterialStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "Material")
	{
	}
	
	/*
	 * Get name
	 *
	 */
	string const& MaterialStream::getName() const
	{
		return mName;
	}

	/*
	 * Get program.
	 *
	 */
	MaterialSpecification::ProgramOptions const& MaterialStream::getProgramOptions() const
	{
		return mQualitySettings[mQualitySetting].spec.program;
	}

	mesh::MeshSpecification const& MaterialStream::getMeshSpecification()
	{
		auto const& qs = mQualitySettings[mQualitySetting];

		if (qs.spec.program.isChild)
		{
			return static_cast<ProgramStream*>(getChildren().at("Program").get())->getMeshSpecification();
		}
		else
		{
			auto res = getResourceMgr()->getResource(qs.spec.program.existingResource);
			return static_cast<Program*>(res.get())->getMeshSpecification();
		}
	}

	/*
	 * Get program uniforms.
	 *
	 */
	UniformCollection const& MaterialStream::getUniforms() const
	{
		return mQualitySettings[mQualitySetting].spec.uniforms;
	}

	/*
	 * Get textures.
	 *
	 */
	std::vector<MaterialSpecification::TextureOptions> const& MaterialStream::getTextures() const
	{
		return mQualitySettings[mQualitySetting].spec.textures;
	}

	uint32_t MaterialStream::createQualitySetting(string const& name)
	{
		auto qualityId = mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}