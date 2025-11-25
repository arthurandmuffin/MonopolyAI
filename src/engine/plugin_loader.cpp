#include "plugin_loader.h"
#include <stdexcept>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

// Implementation of abstract class
class PluginHandleImpl : public PluginHandle {
public:

#ifdef _WIN32
    HMODULE lib_ = nullptr;
#else
    void* lib_ = nullptr;
#endif

    // factory_: pointer to func that takes config and returns agent
    // _ to indicate member var
    AgentExport (*factory_)(const char* config) = nullptr;
    AgentExport cached_ = {};

    explicit PluginHandleImpl(const std::string& library_path) {

#ifdef _WIN32
        lib_ = LoadLibraryA(library_path.c_str());
        if (!lib_) {
            throw std::runtime_error("LoadLibrary failed: " + library_path);
        }
        factory_ = (AgentExport(*)(const char*))GetProcAddress(lib_, "create_agent_export");
#else
        // Load binary in lib_
        lib_ = dlopen(library_path.c_str(), RTLD_NOW);
        if (!lib_) {
            throw std::runtime_error(std::string("dlopen failed: ") + dlerror());
        }
        // Find create_agent function and cast accordingly
        factory_ = (AgentExport(*)(const char*))dlsym(lib_, "create_agent_export");
#endif
        if (!factory_) {
            throw std::runtime_error("Factory symbol not found: create_agent_export");
        }
    }

    // Destructor, unloads the binary
    ~PluginHandleImpl() override {
#ifdef _WIN32
        if (lib_) FreeLibrary(lib_);
#else
        if (lib_) dlclose(lib_);
#endif
    }

    // Get agent + vtable
    AgentExport get_export() override {
        return cached_;
    }

    // Run factory function, resulting agent object stored in cached_
    AgentExport make(const std::string& cfg) override {
        cached_ = factory_(cfg.c_str());
        if (!cached_.vtable.abi_version || cached_.vtable.abi_version() != ABI_VERSION) {
            throw std::runtime_error("ABI version mismatch");
        }
        return cached_;
    }
};

// Function that instantiates the class w/ path
// unique_ptr for automatic memory management
std::unique_ptr<PluginHandle> LoadAgentLibrary(const std::string& path) {
    return std::unique_ptr<PluginHandle>(new PluginHandleImpl(path));
}