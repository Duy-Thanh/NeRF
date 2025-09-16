#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <vector>
#include <mutex>
#include "daf_types.h"

namespace daf {
    // Plugin interface
    class IPlugin {
    public:
        virtual ~IPlugin() = default;
        virtual bool initialize(const std::string& config) = 0;
        virtual bool process(const TaskData& input, TaskResult& output) = 0;
        virtual void shutdown() = 0;
        virtual std::string getName() const = 0;
        virtual std::string getVersion() const = 0;
    };

    // Plugin factory function type
    using PluginFactoryFunc = std::function<std::shared_ptr<IPlugin>()>;

    // Plugin loader class
    class PluginLoader {
    public:
        static PluginLoader& getInstance();
        
        // Load plugin from shared library
        bool loadPlugin(const std::string& pluginPath, const std::string& pluginName);
        
        // Get plugin instance
        std::shared_ptr<IPlugin> getPlugin(const std::string& pluginName);
        
        // Register plugin factory (for static linking)
        bool registerPlugin(const std::string& pluginName, PluginFactoryFunc factory);
        
        // List all loaded plugins
        std::vector<std::string> getLoadedPlugins() const;
        
        // Unload plugin
        bool unloadPlugin(const std::string& pluginName);
        
        // Shutdown all plugins
        void shutdown();
        
    private:
        PluginLoader() = default;
        ~PluginLoader();
        
        struct PluginInfo {
            std::shared_ptr<IPlugin> instance;
            void* libraryHandle;
            PluginFactoryFunc factory;
        };
        
        std::unordered_map<std::string, PluginInfo> plugins_;
        mutable std::mutex pluginsMutex_;
    };
}

// Macro for plugin registration
#define REGISTER_PLUGIN(pluginName, pluginClass) \
    extern "C" { \
        daf::IPlugin* createPlugin() { \
            return new pluginClass(); \
        } \
        void destroyPlugin(daf::IPlugin* plugin) { \
            delete plugin; \
        } \
        const char* getPluginName() { \
            return #pluginName; \
        } \
    }