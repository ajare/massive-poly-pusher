#include <string>

#include "mpp/Resource.h"
#include "mpp/ResourceLifetimeTests.h"

using namespace std;

namespace mpp
{
	namespace
	{
		// A resource with no GPU side, so the reference bookkeeping can be
		// exercised without an OpenGL context.
		class TestResource final : public Resource
		{
		public:

			TestResource()
				: Resource("TestResource", "Test", nullptr, nullptr, nullptr)
			{
			}

		private:

			void createImpl() override {}
			void destroyImpl() override {}
			void loadImpl() override {}
			void unloadImpl() override {}
		};

		class TestWrangler final : public ResourceWrangler
		{
		public:

			explicit TestWrangler(string const& name)
				: ResourceWrangler(name)
			{
			}
		};
	}

	bool runResourceLifetimeTests(string* failure)
	{
		auto fail = [&](char const* message)
		{
			if (failure) *failure = message;
			return false;
		};

		TestResource resource;

		if (describeOutstandingResourceReferences(resource) != "0 outstanding references")
		{
			return fail("an unreferenced resource did not report zero references");
		}

		TestWrangler batch("Batch(Test)");
		TestWrangler model("SceneModel3d(Test)");

		resource.acquire(&batch);
		if (describeOutstandingResourceReferences(resource) != "1 outstanding reference, held by: 'Batch(Test)'")
		{
			return fail("a single holder was not named in the outstanding-reference report");
		}

		resource.acquire(&model);
		auto const both = describeOutstandingResourceReferences(resource);
		if (both.rfind("2 outstanding references, held by:", 0) != 0 ||
			both.find("'Batch(Test)'") == string::npos ||
			both.find("'SceneModel3d(Test)'") == string::npos)
		{
			return fail("multiple holders were not all named in the outstanding-reference report");
		}

		resource.release(&batch);
		if (describeOutstandingResourceReferences(resource) != "1 outstanding reference, held by: 'SceneModel3d(Test)'")
		{
			return fail("the released holder was not removed from the outstanding-reference report");
		}

		bool threw = false;
		try { resource.release(&batch); }
		catch (...) { threw = true; }
		if (!threw) return fail("releasing a resource without acquiring it did not throw");

		resource.release(&model);
		if (describeOutstandingResourceReferences(resource) != "0 outstanding references")
		{
			return fail("the outstanding-reference report did not clear after the last release");
		}

		return true;
	}
}
