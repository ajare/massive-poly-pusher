#pragma once

#include <string>
#include <functional>

#include "mpp/ResourceStream.h"

namespace mpp
{
	class _MPPAPI PostEffectStream : public ResourceStream
	{
		enum class ImageType
		{
			Colour,
			Depth,
			Stencil
		};

		class ImageDimension
		{
			bool mAbsolute;

			size_t mMinValue, mMaxValue;

			union Value
			{
				size_t absoluteValue;
				float percentValue;
			} mValues[3];

		public:

			ImageDimension(bool absolute)
				: mAbsolute(absolute)
			{
			}

			void setMinMax(size_t minValue, size_t maxValue)
			{
				mMinValue = minValue;
				mMaxValue = maxValue;
			}

			size_t getMin() const
			{
				return mMinValue;
			}

			size_t getMax() const
			{
				return mMaxValue;
			}

			size_t getPreferred(int level, size_t windowSize) const
			{
				if (mAbsolute)
				{
					return mValues[level].absoluteValue;
				}
				else
				{
					return windowSize * mValues[level].percentValue
				}
			}
		};

		struct Input
		{
			ImageType type;
		};

		struct Output
		{
			ImageType type;
			string format;
			ImageDimension width, height;
		};

	private:

		std::vector<Input> mInputs;

		Output mOutput;


	private:

		void loadImpl();

	public:

		explicit PostEffectStream(ResourceManager* resourceMgr);

		virtual ~PostEffectStream() = default;

	};
}