#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include "mpp/Batch.h"
#include "mpp/SceneModel3d.h"
#include "mpp/ResourceManager.h"
#include "mpp/RenderSystem.h"

namespace mpp {
void static_log_message(std::string const&, std::string const&) {}
// GPU-free Model backend. The actual Batch code still sees a real Model type.
Model::Model(std::string const& name, RenderSystem* rs, ResourceManager* rm, ResourceStreamPtr stream)
    : Resource(name, "Model", rs, rm, stream) {}
Model::~Model() = default;
void Model::createImpl() {}
void Model::destroyImpl() {}
void Model::loadImpl() {}
void Model::unloadImpl() {}
int Model::getIdCount() const { return 0; }
int Model::getLiveIdCount() const { return 0; }
int Model::getNumMeshes() const { return 0; }
Mesh* Model::getMesh(int) { throw MppException("no meshes in CPU model backend"); }
}
class ProbeMaterial final : public mpp::Resource {
    void createImpl() override {}
    void destroyImpl() override {}
    void loadImpl() override {}
    void unloadImpl() override {}
public:
    explicit ProbeMaterial(std::string const& name) : Resource(name,"BasicMaterial",nullptr,nullptr,{}) {}
};
class BatchProbe final : public mpp::Batch {
    bool indexedVertices() const override { return false; }
    mpp::mesh::Primitive::Type getPrimitiveType() const override { return mpp::mesh::Primitive::Type::Triangles; }
    uint32_t getProgramFlags() const override { return 0; }
    int getIndexWidth() const override { return 32; }
    mpp::mesh::MeshSpecification createMeshSpecification(mpp::mesh::Primitive::Type) override { return {}; }
    mpp::ResourcePtr createMaterial(std::string const& name, mpp::ResourcePtr, uint32_t, bool) override {
        if (failurePoint == 1) throw mpp::MppException("injected material creation failure");
        auto result = std::make_shared<ProbeMaterial>(name);
        mResourceMgr->resources[name] = result;
        return result;
    }
    std::shared_ptr<mpp::ModelStream> createModelStream() override {
        if (failurePoint == 2) throw mpp::MppException("injected model stream failure");
        return {};
    }
    void setMinimumCount(size_t, size_t) override {}
public:
    int failurePoint{0};
    BatchProbe(mpp::RenderSystem* rs, mpp::ResourceManager* rm) : Batch("Regression",0,"","","",{mpp::mesh::Vertex::DataType::None,false},false,rs,rm) {}
    size_t getVertexCount(size_t) const override { return 0; }
    void finishUpdate(size_t,size_t,bool) override {}
};
void require(bool value, char const* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        mpp::RenderSystem rs;
        mpp::ResourceManager rm;
        { BatchProbe neverCreated(&rs,&rm); }
        require(rm.resources.empty() && rm.deleted.empty(),"never-create destructor touched resources");
        std::cout << "PASS never-created Batch destructor is safe\n";
        for (int failurePoint : {1,2,3}) {
            {
                BatchProbe batch(&rs,&rm);
                batch.failurePoint = failurePoint;
                rm.throwOnDeclaration = failurePoint == 3;
                bool threw = false;
                try { batch.create(); } catch (mpp::MppException const&) { threw = true; }
                require(threw,"expected injected creation failure");
                if (failurePoint != 1) require(batch.getMaterial()->getRefCount() == 1,"partial create must acquire material");
            }
            require(rm.resources.empty(),"partial creation destructor stranded its assigned material");
            require(rm.errors.empty(),"partial creation unexpectedly reported about retained resources");
            std::cout << "PASS partial creation failure " << failurePoint << " cleans assigned resources\n";
            rm.throwOnDeclaration = false;
        }
        std::shared_ptr<mpp::SceneModel3d> sceneModel;
        std::shared_ptr<mpp::Resource> material;
        mpp::ResourceWrangler materialOwner("Explicit material owner");
        {
            BatchProbe batch(&rs,&rm);
            rm.nextModel = std::make_shared<mpp::Model>("Regression_Batch_Model", &rs, &rm, nullptr);
            batch.create();
            sceneModel = std::make_shared<mpp::SceneModel3d>(batch.getModel());
            material = batch.getMaterial();
            material->acquire(&materialOwner);
        }
        require(rm.resources.size() == 2,"retained resources must not be forcibly deleted");
        require(rm.errors.size() == 2,"both model and material retention must report");
        require(rm.errors[0].find("Regression_Batch_Model") != std::string::npos && rm.errors[0].find("SceneModel3d") != std::string::npos,"model warning must name resource and remaining SceneModel3d");
        require(rm.errors[1].find("Regression_Batch_Material") != std::string::npos && rm.errors[1].find("Explicit material owner") != std::string::npos,"material warning must name resource and owner");
        require(rm.nextModel->getRefCount() == 1 && material->getRefCount() == 1,"Batch must release only its own acquisitions");
        std::cout << "PASS retained model/material emit diagnostics naming remaining acquirers\n";
        sceneModel.reset();
        material->release(&materialOwner);
        require(rm.nextModel->getRefCount() == 0 && material->getRefCount() == 0,"late owners must release resources");
        require(rm.resources.size() == 2,"diagnostic does not implement deferred deletion");
        rm.deleteResource("Regression_Batch_Model");
        rm.deleteResource("Regression_Batch_Material");
        std::cout << "PASS no deferred deletion is claimed; explicit cleanup removes late-release resources\n";
        { BatchProbe batch(&rs,&rm); rm.nextModel = std::make_shared<mpp::Model>("Regression_Batch_Model",&rs,&rm,nullptr); batch.create(); }
        require(rm.resources.empty(),"unretained full batch destruction must delete both resources");
        std::cout << "PASS complete unretained batch deletes model and material\n";
        return 0;
    } catch (std::exception const& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
