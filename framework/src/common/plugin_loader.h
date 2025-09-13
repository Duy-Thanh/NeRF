#pragma once

#include "daf_types.h"
#include <dlfcn.h>
#include <unordered_map>
#include <mutex>

namespace daf {

class PluginLoader {
public:
    static PluginLoader& Instance();
    
    // Load plugin from shared library
    Result<std::unique_ptr<IPlugin>> LoadPlugin(const std::string& plugin_path);
    
    // Unload plugin
    bool UnloadPlugin(const std::string& plugin_name);
    
    // Get loaded plugin info
    std::vector<std::string> GetLoadedPlugins() const;
    bool IsPluginLoaded(const std::string& plugin_name) const;
    
    ~PluginLoader();

private:
    PluginLoader() = default;
    
    struct PluginInfo {
        void* handle;
        std::string name;
        std::string version;
        std::string path;
        
        // Function pointers
        IPlugin* (*create_func)();
        void (*destroy_func)(IPlugin*);
        const char* (*get_name_func)();
        const char* (*get_version_func)();
    };
    
    mutable std::mutex mutex_;
    std::unordered_map<std::string, PluginInfo> loaded_plugins_;
    
    // Helper methods
    bool LoadPluginSymbols(PluginInfo& info);
    void UnloadPluginInternal(const std::string& plugin_name);
};

// RAII wrapper for plugin instances
class PluginInstance {
public:
    PluginInstance(std::unique_ptr<IPlugin> plugin, const std::string& name);
    ~PluginInstance();
    
    // Move constructor and assignment
    PluginInstance(PluginInstance&& other) noexcept;
    PluginInstance& operator=(PluginInstance&& other) noexcept;
    
    // Disable copy constructor and assignment
    PluginInstance(const PluginInstance&) = delete;
    PluginInstance& operator=(const PluginInstance&) = delete;
    
    IPlugin* Get() const { return plugin_.get(); }
    IPlugin* operator->() const { return plugin_.get(); }
    IPlugin& operator*() const { return *plugin_; }
    
    bool IsValid() const { return plugin_ != nullptr; }
    const std::string& GetName() const { return name_; }

private:
    std::unique_ptr<IPlugin> plugin_;
    std::string name_;
};

} // namespace daf
