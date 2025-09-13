#include "plugin_loader.h"
#include <iostream>
#include <filesystem>

namespace daf {

PluginLoader& PluginLoader::Instance() {
    static PluginLoader instance;
    return instance;
}

Result<std::unique_ptr<IPlugin>> PluginLoader::LoadPlugin(const std::string& plugin_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!std::filesystem::exists(plugin_path)) {
        return Result<std::unique_ptr<IPlugin>>(
            ErrorCode::IO_ERROR, 
            "Plugin file not found: " + plugin_path
        );
    }
    
    // Load the shared library
    void* handle = dlopen(plugin_path.c_str(), RTLD_LAZY);
    if (!handle) {
        return Result<std::unique_ptr<IPlugin>>(
            ErrorCode::PLUGIN_ERROR,
            "Failed to load plugin library: " + std::string(dlerror())
        );
    }
    
    // Create plugin info
    PluginInfo info;
    info.handle = handle;
    info.path = plugin_path;
    
    // Load symbols
    if (!LoadPluginSymbols(info)) {
        dlclose(handle);
        return Result<std::unique_ptr<IPlugin>>(
            ErrorCode::PLUGIN_ERROR,
            "Failed to load plugin symbols from: " + plugin_path
        );
    }
    
    // Get plugin name and version
    info.name = info.get_name_func();
    info.version = info.get_version_func();
    
    // Check if plugin already loaded
    if (loaded_plugins_.find(info.name) != loaded_plugins_.end()) {
        dlclose(handle);
        return Result<std::unique_ptr<IPlugin>>(
            ErrorCode::INVALID_STATE,
            "Plugin already loaded: " + info.name
        );
    }
    
    // Create plugin instance
    IPlugin* plugin_ptr = info.create_func();
    if (!plugin_ptr) {
        dlclose(handle);
        return Result<std::unique_ptr<IPlugin>>(
            ErrorCode::PLUGIN_ERROR,
            "Failed to create plugin instance: " + info.name
        );
    }
    
    std::unique_ptr<IPlugin> plugin(plugin_ptr);
    
    // Store plugin info
    loaded_plugins_[info.name] = std::move(info);
    
    std::cout << "Loaded plugin: " << info.name << " v" << info.version << std::endl;
    
    return Result<std::unique_ptr<IPlugin>>(std::move(plugin));
}

bool PluginLoader::UnloadPlugin(const std::string& plugin_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    UnloadPluginInternal(plugin_name);
    return true;
}

std::vector<std::string> PluginLoader::GetLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> plugins;
    
    for (const auto& pair : loaded_plugins_) {
        plugins.push_back(pair.first);
    }
    
    return plugins;
}

bool PluginLoader::IsPluginLoaded(const std::string& plugin_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return loaded_plugins_.find(plugin_name) != loaded_plugins_.end();
}

PluginLoader::~PluginLoader() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Unload all plugins
    for (const auto& pair : loaded_plugins_) {
        dlclose(pair.second.handle);
    }
    
    loaded_plugins_.clear();
}

bool PluginLoader::LoadPluginSymbols(PluginInfo& info) {
    // Load required function symbols
    info.create_func = (IPlugin*(*)())dlsym(info.handle, "CreatePlugin");
    if (!info.create_func) {
        std::cerr << "Symbol 'CreatePlugin' not found: " << dlerror() << std::endl;
        return false;
    }
    
    info.destroy_func = (void(*)(IPlugin*))dlsym(info.handle, "DestroyPlugin");
    if (!info.destroy_func) {
        std::cerr << "Symbol 'DestroyPlugin' not found: " << dlerror() << std::endl;
        return false;
    }
    
    info.get_name_func = (const char*(*)())dlsym(info.handle, "GetPluginName");
    if (!info.get_name_func) {
        std::cerr << "Symbol 'GetPluginName' not found: " << dlerror() << std::endl;
        return false;
    }
    
    info.get_version_func = (const char*(*)())dlsym(info.handle, "GetPluginVersion");
    if (!info.get_version_func) {
        std::cerr << "Symbol 'GetPluginVersion' not found: " << dlerror() << std::endl;
        return false;
    }
    
    return true;
}

void PluginLoader::UnloadPluginInternal(const std::string& plugin_name) {
    auto it = loaded_plugins_.find(plugin_name);
    if (it != loaded_plugins_.end()) {
        dlclose(it->second.handle);
        loaded_plugins_.erase(it);
        std::cout << "Unloaded plugin: " << plugin_name << std::endl;
    }
}

// PluginInstance implementation
PluginInstance::PluginInstance(std::unique_ptr<IPlugin> plugin, const std::string& name)
    : plugin_(std::move(plugin)), name_(name) {
}

PluginInstance::~PluginInstance() {
    if (plugin_) {
        plugin_->Shutdown();
    }
}

PluginInstance::PluginInstance(PluginInstance&& other) noexcept
    : plugin_(std::move(other.plugin_)), name_(std::move(other.name_)) {
}

PluginInstance& PluginInstance::operator=(PluginInstance&& other) noexcept {
    if (this != &other) {
        if (plugin_) {
            plugin_->Shutdown();
        }
        plugin_ = std::move(other.plugin_);
        name_ = std::move(other.name_);
    }
    return *this;
}

} // namespace daf
