#include <GL/glew.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "mpp/RenderGraphExecutor.h"
#include "mpp/SceneModel3d.h"
#include "mpp/Resource.h"
#include "mpp/ResourceLifetimeTests.h"
#ifdef ISSUE14_FIXED
#include "RenderGraphFrameScope.h"
#endif

// No GPU execution occurs in this ownership test. These globals are the only
// graphics dependencies of the production executor's constructor/destructor.
GLboolean __GLEW_VERSION_3_3 = GL_FALSE;
GLboolean __GLEW_ARB_timer_query = GL_FALSE;
PFNGLDELETEQUERIESPROC __glewDeleteQueries = nullptr;
namespace mpp { void static_log_message(std::string const&, std::string const&) {} }

class ProbeResource final : public mpp::Resource {
    void createImpl() override {}
    void destroyImpl() override {}
    void loadImpl() override {}
    void unloadImpl() override {}
public:
    ProbeResource() : Resource("Regression_Batch_Model", "CPU probe", nullptr, nullptr, {}) {}
};
void require(bool condition, char const* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        std::string diagnosticFailure;
        if (!mpp::runResourceLifetimeTests(&diagnosticFailure))
            throw std::runtime_error(diagnosticFailure);
        // The callback-management methods never dereference this token.
        alignas(void*) unsigned char renderSystemToken[sizeof(void*)]{};
        mpp::RenderGraphExecutor executor(reinterpret_cast<mpp::RenderSystem*>(renderSystemToken));
        auto resource = std::make_shared<ProbeResource>();
        mpp::ResourceWrangler batchOwner("Regression_Batch");
        resource->acquire(&batchOwner);
        for (int cycle = 0; cycle < 10; ++cycle) {
        for (int failurePoint : {0, 1, 2}) {
            std::weak_ptr<mpp::SceneModel3d> observed;
            try {
#ifdef ISSUE14_FIXED
                mpp::detail::RenderGraphFrameScope frameScope(executor, true);
#endif
                auto model = std::make_shared<mpp::SceneModel3d>(resource);
                observed = model;
                require(resource->getRefCount() == 2, "scene model must acquire production Resource");
                std::vector<mpp::SceneModel3dPtr> models{model};
                executor.setPassCallback("Scene", [models](auto const&) {});
                for (int face = 0; face < 6; ++face)
                    executor.setPassCallback("ShadowFace" + std::to_string(face), [models](auto const&) {});
                models.clear();
                model.reset();
                require(!observed.expired(), "registered production callbacks must retain model until frame ends");
                if (failurePoint == 1) throw std::runtime_error("injected failure before execute");
                if (failurePoint == 2) executor.setPassCallback("", [](auto const&) {});
            } catch (std::runtime_error const& error) {
                if (std::string(error.what()) != "injected failure before execute" &&
                    std::string(error.what()) != "Render graph pass callback requires a pass name and function.") throw;
            }
#ifdef ISSUE14_FIXED
            require(observed.expired(), "frame scope retained the scene model after frame end");
            require(resource->getRefCount() == 1, "scene model failed to release production Resource on frame end");
            if (cycle == 0) std::cout << "PASS " << (failurePoint == 0 ? "normal" : failurePoint == 1 ? "exception" : "partial registration failure") << " frame releases SceneModel3d while executor lives\n";
#else
            require(!observed.expired(), "baseline no longer exhibits issue #14 ownership retention");
            require(resource->getRefCount() == 2, "baseline expected outstanding SceneModel3d resource acquirer");
            if (cycle == 0) std::cout << "REPRODUCED " << (failurePoint == 0 ? "normal" : failurePoint == 1 ? "exception" : "partial registration failure") << " frame retains SceneModel3d and model acquisition\n";
            executor.clearPassCallbacks();
            require(observed.expired(), "clearing actual callback table must release scene model");
#endif
        }
        }
        std::cout << "PASS all 30 frame lifecycles completed (10 repeated cycles)\n";
#ifdef ISSUE14_FIXED
        // XML/standalone executor callbacks intentionally persist across frames.
        std::weak_ptr<mpp::SceneModel3d> persistent;
        {
            mpp::detail::RenderGraphFrameScope frameScope(executor, false);
            auto model = std::make_shared<mpp::SceneModel3d>(resource);
            persistent = model;
            executor.setPassCallback("Persistent", [model](auto const&) {});
        }
        require(!persistent.expired(), "persistent callbacks were unintentionally cleared");
        executor.clearPassCallbacks();
        require(persistent.expired(), "explicit callback cleanup failed");
        std::cout << "PASS persistent executor callbacks preserved when cleanup is disabled\n";
#endif
        resource->release(&batchOwner);
        require(resource->getRefCount() == 0, "resource refcount not zero at batch teardown");
        return 0;
    } catch (std::exception const& error) {
        std::cerr << "FAIL " << error.what() << '\n';
        return 1;
    }
}
