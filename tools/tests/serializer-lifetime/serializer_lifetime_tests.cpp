#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "mpp/resource-parsers/Serializer.h"

namespace
{
	using mpp::resource_parsers::Serializer;
	using mpp::resource_parsers::SerializerPtr;

	void require(bool condition, char const* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	struct LoadFailure {};

	class TrackingSerializer : public Serializer
	{
		int& mDestroyed;
		std::shared_ptr<int> mPayload;

	public:
		TrackingSerializer(int& destroyed, std::shared_ptr<int> payload)
			: mDestroyed(destroyed), mPayload(std::move(payload)) {}

		// Intentionally no "override": the unfixed header must still compile
		// so the contract check below can fail without executing undefined behavior.
		~TrackingSerializer() { ++mDestroyed; }

		void loadFromFile(std::string const& path) override
		{
			mData.addEntry("path", path);
			if (path == "throw") throw LoadFailure{};
		}
	};

	void checkLifetime(bool failLoad, bool shareOwnership)
	{
		int destroyed = 0;
		auto payload = std::make_shared<int>(42);
		std::weak_ptr<int> weakPayload = payload;
		SerializerPtr retained;
		bool caught = false;
		try
		{
			// Match FileStream's critical ownership seam: the factory erases the
			// derived pointer BEFORE shared_ptr selects its default deleter.
			std::function<Serializer*()> factory = [&]() -> Serializer*
			{
				return new TrackingSerializer(destroyed, std::move(payload));
			};
			SerializerPtr serializer(factory());
			if (shareOwnership) retained = serializer;
			serializer->loadFromFile(failLoad ? "throw" : "fixture.yaml");
			require(serializer->getData().getEntry("path").getValue() == "fixture.yaml",
				"virtual load and structured data access must still work");
		}
		catch (LoadFailure const&)
		{
			caught = true;
		}
		require(caught == failLoad, "load exception must propagate");
		require(!payload, "the derived object must exclusively own its payload");
		if (shareOwnership)
		{
			require(destroyed == 0 && !weakPayload.expired(),
				"shared ownership must keep the derived serializer alive");
			retained.reset();
		}
		require(destroyed == 1, "last base-pointer owner must destroy the derived serializer exactly once");
		require(weakPayload.expired(), "derived-owned resources must be released");
	}
}

int main()
{
	try
	{
		// A deterministic baseline failure on all compilers, rather than relying
		// on one library's UB trap or silently accepting a skipped destructor.
		require(std::has_virtual_destructor_v<Serializer>,
			"Serializer needs a virtual destructor: FileStream owns derived objects through Serializer*");
		for (int cycle = 0; cycle < 10; ++cycle)
		{
			checkLifetime(false, false);
			checkLifetime(true, false);
			checkLifetime(false, true);
			checkLifetime(true, true);
		}
		std::cout << "PASS: 40 serializer lifecycles (normal, exceptional, and shared ownership)\n";
		return 0;
	}
	catch (std::exception const& error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
