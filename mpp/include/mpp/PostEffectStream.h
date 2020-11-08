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
			struct Value
			{
				bool absolute;

				union ValueData
				{
					size_t absoluteValue;
					float percentValue;
				} value;

				size_t get(size_t referenceSize) const
				{
					return absolute ? value.absoluteValue : (size_t)(referenceSize * value.percentValue);
				}
			};

			Value mMin, mMax, mPreferred;


		public:

			ImageDimension()
			{
				setMinValueAbsolute(0);
			}

			void setMinValueAbsolute(size_t minValue)
			{
				mMin.absolute = true;
				mMin.value.absoluteValue = minValue;
			}

			void setMinValueRelative(float minValue)
			{
				mMin.absolute = false;
				mMin.value.percentValue = minValue;
			}

			void setMaxValueAbsolute(size_t minValue)
			{
				mMax.absolute = true;
				mMax.value.absoluteValue = minValue;
			}

			void setMaxValueRelative(float minValue)
			{
				mMax.absolute = false;
				mMax.value.percentValue = minValue;
			}

			size_t getMin(size_t windowSize) const
			{
				return mMin.get(windowSize);
			}

			size_t getMax(size_t windowSize) const
			{
				return mMax.get(windowSize);
			}

			size_t getPreferred(size_t windowSize) const
			{
				return mPreferred.get(windowSize);
			}
		};

		struct Input
		{
			ImageType type;
		};

		struct Output
		{
			ImageType type;
			std::string format;
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