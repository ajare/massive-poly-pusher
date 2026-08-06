#include "mpp/PbrMaterialStream.h"
#include "mpp/ProgramStream.h"
#include "mpp/ResourceManager.h"

using namespace std;

namespace mpp
{

	/*
	 * Constructor.
	 *
	 */
	PbrMaterialStream::PbrMaterialStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "PbrMaterial")
	{
	}

	/*
	 * Get name
	 *
	 */
	string const& PbrMaterialStream::getName() const
	{
		return mName;
	}

	/*
	 * Get program.
	 *
	 */
	PbrMaterialSpecification::ProgramOptions const& PbrMaterialStream::getProgramOptions() const
	{
		return mQualitySettings[mQualitySetting].spec.program;
	}

	mesh::MeshSpecification const& PbrMaterialStream::getMeshSpecification()
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
	UniformCollection const& PbrMaterialStream::getUniforms() const
	{
		return mQualitySettings[mQualitySetting].spec.uniforms;
	}

	PbrMaterialSpecification::PbrSurface const& PbrMaterialStream::getPbrSurface() const
	{
		return mQualitySettings[mQualitySetting].spec.pbr;
	}

	/*
	 * Get textures.
	 *
	 */
	std::vector<PbrMaterialSpecification::TextureOptions> const& PbrMaterialStream::getTextures() const
	{
		return mQualitySettings[mQualitySetting].spec.textures;
	}

	uint32_t PbrMaterialStream::createQualitySetting(string const& name)
	{
		auto qualityId = (uint32_t)mQualitySettings.size();
		mQualityNames[name] = qualityId;

		mQualitySettings.push_back(QualitySetting());
		return qualityId;
	}
}