#include "mpp/BasicMaterialStream.h"

using namespace std;

namespace mpp
{
	BasicMaterialStream::BasicMaterialStream(ResourceManager* resourceMgr)
		: ResourceStream(resourceMgr, "BasicMaterial")
	{
	}

	string const& BasicMaterialStream::getName() const { return mName; }

	BasicMaterialSpecification::ProgramOptions const& BasicMaterialStream::getProgramOptions() const
	{
		return mQualitySettings[mQualitySetting].spec.program;
	}

	mesh::MeshSpecification const& BasicMaterialStream::getMeshSpecification()
	{
		return mQualitySettings[mQualitySetting].spec.program.spec;
	}

	UniformCollection const& BasicMaterialStream::getUniforms() const
	{
		return mQualitySettings[mQualitySetting].spec.uniforms;
	}

	vector<BasicMaterialSpecification::TextureOptions> const& BasicMaterialStream::getTextures() const
	{
		return mQualitySettings[mQualitySetting].spec.textures;
	}

	uint32_t BasicMaterialStream::createQualitySetting(string const& name)
	{
		uint32_t quality = (uint32_t)mQualitySettings.size();
		mQualityNames[name] = quality;
		mQualitySettings.push_back(QualitySetting());
		return quality;
	}
}
