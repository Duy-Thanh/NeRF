#include "plugin_loader.h"
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <mutex>

namespace daf {

PluginLoader& PluginLoader::getInstance() {
    static PluginLoader instance;
    return instance;
}

PluginLoader::~PluginLoader() {
    shutdown();
}

bool PluginLoader::loadPlugin(const std::string& pluginPath, const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    if (plugins_.find(pluginName) != plugins_.end()) {
        std::cout << "Plugin " << pluginName << " already loaded" << std::endl;
        return true;
    }
    
    if (!std::filesystem::exists(pluginPath)) {
        std::cerr << "Plugin file not found: " << pluginPath << std::endl;
        return false;
    }
    
    // Load the shared library
    void* handle = dlopen(pluginPath.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Cannot load plugin " << pluginPath << ": " << dlerror() << std::endl;
        return false;
    }
    
    // Clear any existing error
    dlerror();
    
    // Load the create function
    typedef IPlugin* (*createPlugin_t)();
    createPlugin_t createPlugin = (createPlugin_t) dlsym(handle, "createPlugin");
    
    const char* dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol createPlugin: " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Load the destroy function
    typedef void (*destroyPlugin_t)(IPlugin*);
    destroyPlugin_t destroyPlugin = (destroyPlugin_t) dlsym(handle, "destroyPlugin");
    
    dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Cannot load symbol destroyPlugin: " << dlsym_error << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Create plugin instance
    std::shared_ptr<IPlugin> plugin(createPlugin(), destroyPlugin);
    if (!plugin) {
        std::cerr << "Failed to create plugin instance" << std::endl;
        dlclose(handle);
        return false;
    }
    
    // Store plugin info
    PluginInfo info;
    info.instance = plugin;
    info.libraryHandle = handle;
    info.factory = nullptr;
    
    plugins_[pluginName] = std::move(info);
    
    std::cout << "Successfully loaded plugin: " << pluginName << std::endl;
    return true;
}

std::shared_ptr<IPlugin> PluginLoader::getPlugin(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = plugins_.find(pluginName);
    if (it != plugins_.end()) {
        return it->second.instance;
    }
    
    return nullptr;
}

bool PluginLoader::registerPlugin(const std::string& pluginName, PluginFactoryFunc factory) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    if (plugins_.find(pluginName) != plugins_.end()) {
        std::cout << "Plugin " << pluginName << " already registered" << std::endl;
        return true;
    }
    
    // Create plugin instance using factory
    auto plugin = factory();
    if (!plugin) {
        std::cerr << "Failed to create plugin instance using factory" << std::endl;
        return false;
    }
    
    // Store plugin info
    PluginInfo info;
    info.instance = plugin;
    info.libraryHandle = nullptr;
    info.factory = factory;
    
    plugins_[pluginName] = std::move(info);
    
    std::cout << "Successfully registered plugin: " << pluginName << std::endl;
    return true;
}

std::vector<std::string> PluginLoader::getLoadedPlugins() const {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    std::vector<std::string> pluginNames;
    for (const auto& pair : plugins_) {
        pluginNames.push_back(pair.first);
    }
    
    return pluginNames;
}

bool PluginLoader::unloadPlugin(const std::string& pluginName) {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    auto it = plugins_.find(pluginName);
    if (it == plugins_.end()) {
        return false;
    }
    
    // Shutdown plugin if it exists
    if (it->second.instance) {
        it->second.instance->shutdown();
        it->second.instance.reset();
    }
    
    // Close library handle if it was dynamically loaded
    if (it->second.libraryHandle) {
        dlclose(it->second.libraryHandle);
    }
    
    plugins_.erase(it);
    std::cout << "Successfully unloaded plugin: " << pluginName << std::endl;
    return true;
}

void PluginLoader::shutdown() {
    std::lock_guard<std::mutex> lock(pluginsMutex_);
    
    for (auto& pair : plugins_) {
        if (pair.second.instance) {
            pair.second.instance->shutdown();
            pair.second.instance.reset();
        }
        
        if (pair.second.libraryHandle) {
            dlclose(pair.second.libraryHandle);
        }
    }
    
    plugins_.clear();
    std::cout << "All plugins shut down" << std::endl;
}

}