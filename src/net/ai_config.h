// MonoSprite AI Client - Configuration Persistence
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef NET_AI_CONFIG_H_INCLUDED
#define NET_AI_CONFIG_H_INCLUDED
#pragma once

#include "net/ai_provider.h"

#include <string>
#include <vector>

namespace net {

// Manages reading/writing AI provider settings from the app's .ini file.
//
// Config file section layout:
//   [ai]
//   provider_type = 0       ; AiProviderType enum value
//   base_url = ...
//   api_key = ...
//   selected_model = ...
//   sidebar_visible = true
//   auto_pixel_prompt = true
//
class AiConfig {
public:
  AiConfig();
  ~AiConfig();

  // Load all AI settings from the app's .ini config file.
  void load();

  // Save all AI settings to the app's .ini config file.
  void save();

  // Provider settings
  AiProviderType providerType() const { return m_providerType; }
  void setProviderType(AiProviderType type);

  const std::string& baseUrl() const { return m_baseUrl; }
  void setBaseUrl(const std::string& url);

  const std::string& apiKey() const { return m_apiKey; }
  void setApiKey(const std::string& key);

  const std::string& selectedModelId() const { return m_selectedModelId; }
  void setSelectedModelId(const std::string& modelId);

  // UI preferences
  bool sidebarVisible() const { return m_sidebarVisible; }
  void setSidebarVisible(bool visible);

  // When true, the prompt will automatically append pixel art keywords
  // (e.g., "pixel art, 8-bit, clean outline") to user prompts.
  bool autoPixelPrompt() const { return m_autoPixelPrompt; }
  void setAutoPixelPrompt(bool enabled);

  // Get the full provider config for the current selection
  AiProviderConfig currentProviderConfig() const;

  // Returns true if the current provider requires an API key
  bool currentProviderRequiresApiKey() const;

private:
  AiProviderType m_providerType;
  std::string m_baseUrl;
  std::string m_apiKey;
  std::string m_selectedModelId;
  bool m_sidebarVisible;
  bool m_autoPixelPrompt;
};

} // namespace net

#endif // NET_AI_CONFIG_H_INCLUDED
