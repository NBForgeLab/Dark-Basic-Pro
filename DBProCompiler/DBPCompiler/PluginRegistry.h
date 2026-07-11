#pragma once
#include <windows.h>
#include <string>
#include <unordered_map>

class PluginRegistry {
public:
    static PluginRegistry& GetInstance() {
        static PluginRegistry instance;
        return instance;
    }

    void RegisterPlugin(const std::string& name, HINSTANCE handle) {
        m_plugins[name] = handle;
    }

    HINSTANCE GetPlugin(const std::string& name) const {
        auto it = m_plugins.find(name);
        return (it != m_plugins.end()) ? it->second : nullptr;
    }

    void Clear() {
        m_plugins.clear();
    }

private:
    PluginRegistry() = default;
    ~PluginRegistry() = default;
    PluginRegistry(const PluginRegistry&) = delete;
    PluginRegistry& operator=(const PluginRegistry&) = delete;

    std::unordered_map<std::string, HINSTANCE> m_plugins;
};
