// MonoSprite AI Client - Configuration Persistence
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "net/ai_config.h"
#include "app/ini_file.h"

namespace net {

namespace {
  const char* kSection = "ai";
}

AiConfig::AiConfig()
  : m_providerType(AiProviderType::LocalComfyUI)
  , m_baseUrl("http://127.0.0.1:8188")
  , m_selectedModelId("")
  , m_sidebarVisible(true)
  , m_autoPixelPrompt(true)
{
}

AiConfig::~AiConfig()
{
}

void AiConfig::load()
{
  using namespace app;

  int typeVal = get_config_int(kSection, "provider_type",
                               static_cast<int>(AiProviderType::LocalComfyUI));
  m_providerType = static_cast<AiProviderType>(typeVal);

  m_baseUrl = get_config_string(kSection, "base_url",
                                "http://127.0.0.1:8188");
  m_apiKey = get_config_string(kSection, "api_key", "");
  m_selectedModelId = get_config_string(kSection, "selected_model", "");
  m_sidebarVisible = get_config_bool(kSection, "sidebar_visible", true);
  m_autoPixelPrompt = get_config_bool(kSection, "auto_pixel_prompt", true);

  // If no base URL was set, use the default for this provider
  if (m_baseUrl.empty()) {
    auto defaults = getDefaultProviders();
    for (auto& p : defaults) {
      if (p.type == m_providerType) {
        m_baseUrl = p.baseUrl;
        break;
      }
    }
  }
}

void AiConfig::save()
{
  using namespace app;

  set_config_int(kSection, "provider_type",
                 static_cast<int>(m_providerType));
  set_config_string(kSection, "base_url", m_baseUrl.c_str());
  set_config_string(kSection, "api_key", m_apiKey.c_str());
  set_config_string(kSection, "selected_model", m_selectedModelId.c_str());
  set_config_bool(kSection, "sidebar_visible", m_sidebarVisible);
  set_config_bool(kSection, "auto_pixel_prompt", m_autoPixelPrompt);

  flush_config_file();
}

void AiConfig::setProviderType(AiProviderType type)
{
  m_providerType = type;

  // Reset base URL to default for the new provider
  auto defaults = getDefaultProviders();
  for (auto& p : defaults) {
    if (p.type == type) {
      m_baseUrl = p.baseUrl;
      break;
    }
  }
}

void AiConfig::setBaseUrl(const std::string& url)
{
  m_baseUrl = url;
}

void AiConfig::setApiKey(const std::string& key)
{
  m_apiKey = key;
}

void AiConfig::setSelectedModelId(const std::string& modelId)
{
  m_selectedModelId = modelId;
}

void AiConfig::setSidebarVisible(bool visible)
{
  m_sidebarVisible = visible;
}

void AiConfig::setAutoPixelPrompt(bool enabled)
{
  m_autoPixelPrompt = enabled;
}

AiProviderConfig AiConfig::currentProviderConfig() const
{
  auto defaults = getDefaultProviders();
  for (auto& p : defaults) {
    if (p.type == m_providerType) {
      AiProviderConfig config = p;
      config.baseUrl = m_baseUrl;
      config.apiKey = m_apiKey;
      return config;
    }
  }

  // Fallback: custom provider
  AiProviderConfig config;
  config.type = m_providerType;
  config.displayName = "Custom";
  config.baseUrl = m_baseUrl;
  config.apiKey = m_apiKey;
  config.requiresApiKey = !m_apiKey.empty();
  return config;
}

bool AiConfig::currentProviderRequiresApiKey() const
{
  auto defaults = getDefaultProviders();
  for (auto& p : defaults) {
    if (p.type == m_providerType) {
      return p.requiresApiKey;
    }
  }
  return false;
}

} // namespace net
