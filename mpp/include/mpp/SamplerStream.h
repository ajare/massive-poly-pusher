#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"
#include "mpp/SamplerParams.h"

namespace mpp
{

	class _MPPAPI SamplerStream : public ResourceStream
	{
	public:

		enum class MinFilter
		{
			Nearest,
			Linear,
			NearestMipmapNearest,
			LinearMipmapNearest,
			NearestMipmapLinear,
			LinearMipmapLinear
		};

		enum class MagFilter
		{
			Nearest,
			Linear
		};

		enum class Wrapping
		{
			Repeat,
			MirroredRepeat,
			ClampToEdge,
			ClampToBorder
		};

	private:

		struct QualitySetting
		{
			SamplerParams params;
		};

	protected:

		std::vector<QualitySetting> mQualitySettings;

	protected:

		void loadImpl();

	public:

		SamplerStream(ResourceManager* resourceMgr);

		virtual ~SamplerStream();

		SamplerParams const& getParams(uint32_t quality) const;

		uint32_t createQualitySetting(std::string const& name);
	};
}