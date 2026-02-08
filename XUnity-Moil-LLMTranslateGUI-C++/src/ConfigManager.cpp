#include "ConfigManager.h"

// 实现加载配置的函数
AppConfig ConfigManager::loadConfig(const QString& filename) {
    QSettings settings(filename, QSettings::IniFormat);
    AppConfig config;

    config.api_address = settings.value("Settings/api_address", config.api_address).toString();
    config.api_key = settings.value("Settings/api_key", config.api_key).toString();
    config.model_name = settings.value("Settings/model_name", config.model_name).toString();
    config.port = settings.value("Settings/port", config.port).toInt();
    config.system_prompt = settings.value("Settings/system_prompt", config.system_prompt).toString();
    config.pre_prompt = settings.value("Settings/pre_prompt", config.pre_prompt).toString();
    config.context_num = settings.value("Settings/context_num", config.context_num).toInt();
    config.temperature = settings.value("Settings/temperature", config.temperature).toDouble();
    config.max_threads = settings.value("Settings/max_threads", config.max_threads).toInt();
    config.language = settings.value("Settings/language", config.language).toInt();
    
    // --- 术语表相关设置 ---
    config.enable_glossary = settings.value("Settings/enable_glossary", config.enable_glossary).toBool();
    
    config.glossary_path = settings.value("Settings/glossary_path", config.glossary_path).toString();
    config.glossary_history = settings.value("Settings/glossary_history").toStringList();
    
    return config;
}

// 实现保存配置的函数
void ConfigManager::saveConfig(const AppConfig& config, const QString& filename) {
    QSettings settings(filename, QSettings::IniFormat);

    settings.setValue("Settings/api_address", config.api_address);
    settings.setValue("Settings/api_key", config.api_key);
    settings.setValue("Settings/model_name", config.model_name);
    settings.setValue("Settings/port", config.port);
    settings.setValue("Settings/system_prompt", config.system_prompt);
    settings.setValue("Settings/pre_prompt", config.pre_prompt);
    settings.setValue("Settings/context_num", config.context_num);
    settings.setValue("Settings/temperature", config.temperature);
    settings.setValue("Settings/max_threads", config.max_threads);
    settings.setValue("Settings/language", config.language);
    
    // --- 术语表相关设置 ---
    settings.setValue("Settings/enable_glossary", config.enable_glossary);
    
    settings.setValue("Settings/glossary_path", config.glossary_path);
    settings.setValue("Settings/glossary_history", config.glossary_history);
    
    settings.sync();
}