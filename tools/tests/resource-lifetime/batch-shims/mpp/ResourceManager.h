#pragma once
#include <map>
#include <string>
#include <vector>
#include "mpp/Resource.h"
#include "mpp/MppException.h"
#include "mpp/mesh/MeshSpecification.h"
namespace mpp {
class ResourceManager {
public:
    std::map<std::string, ResourcePtr> resources;
    std::vector<std::string> deleted;
    std::vector<std::string> errors;
    void errorMessage(std::string const& message) { errors.push_back(message); }
    ResourcePtr nextModel;
    bool throwOnDeclaration{false};
    std::pair<ResourcePtr, bool> declareResource(std::string const& name, ResourceStreamPtr, bool = true) {
        if (throwOnDeclaration) throw MppException("injected duplicate declaration failure");
        resources[name] = nextModel;
        return {nextModel, true};
    }
    ResourcePtr getResource(std::string const& name, bool = false) { return resources.at(name); }
    void deleteResource(std::string const& name) {
        auto resource = resources.at(name);
        if (resource->getRefCount()) throw MppException("test backend refuses deletion of acquired resource");
        resource->destroy();
        resources.erase(name);
        deleted.push_back(name);
    }
    ResourcePtr getDefault2dProgram(std::string const&, std::string const&, mesh::MeshSpecification const&, uint32_t, bool, std::string) { throw MppException("unused backend method"); }
    ResourcePtr getDefault3dProgram(mesh::MeshSpecification const&, uint32_t, bool, std::string) { throw MppException("unused backend method"); }
};
}
