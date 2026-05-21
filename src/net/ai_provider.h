// MonoSprite AI Client - Provider Configuration
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef NET_AI_PROVIDER_H_INCLUDED
#define NET_AI_PROVIDER_H_INCLUDED
#pragma once

#include <string>
#include <vector>

namespace net {

// Represents a supported AI provider backend.
// Each provider has its own API format, authentication, and model catalog.
enum class AiProviderType {
  LocalComfyUI,      // Local ComfyUI server (free, no API key needed)
  LocalAutomatic1111, // Local Automatic1111/SDUI server (free, no API key needed)
  FalAi,             // fal.ai cloud API
  Replicate,         // Replicate cloud API
  StabilityAi,       // Stability AI cloud API
  Custom             // User-defined endpoint
};

// Describes one AI model available from a provider.
struct AiModelInfo {
  std::string id;          // Machine-readable ID (e.g., "fal-ai/flux/schnell")
  std::string displayName; // Human-readable name shown in dropdown (e.g., "FLUX.1 Schnell")
  std::string description; // Short description of what the model does
  bool supportsTextToImage;
  bool supportsImageToImage;
  bool supportsAnimation;
  bool supportsPaletteGen;
  bool supportsSegmentation;
  bool supportsInpainting;

  AiModelInfo()
    : supportsTextToImage(false)
    , supportsImageToImage(false)
    , supportsAnimation(false)
    , supportsPaletteGen(false)
    , supportsSegmentation(false)
    , supportsInpainting(false) {}
};

// Stores all configuration for a single AI provider connection.
// This gets persisted to the app's .ini config file.
struct AiProviderConfig {
  AiProviderType type;
  std::string displayName;  // e.g., "fal.ai", "Local ComfyUI"
  std::string baseUrl;      // e.g., "https://queue.fal.run" or "http://127.0.0.1:8188"
  std::string apiKey;       // API key (empty for local providers)
  bool requiresApiKey;      // Whether this provider needs an API key

  AiProviderConfig()
    : type(AiProviderType::Custom)
    , requiresApiKey(false) {}

  AiProviderConfig(AiProviderType type,
                   const std::string& displayName,
                   const std::string& baseUrl,
                   bool requiresApiKey)
    : type(type)
    , displayName(displayName)
    , baseUrl(baseUrl)
    , requiresApiKey(requiresApiKey) {}
};

// Returns a list of all built-in providers with default settings.
// The user can modify baseUrl/apiKey in the sidebar settings panel.
inline std::vector<AiProviderConfig> getDefaultProviders() {
  return {
    { AiProviderType::LocalComfyUI,
      "Local ComfyUI",
      "http://127.0.0.1:8188",
      false },
    { AiProviderType::LocalAutomatic1111,
      "Local Stable Diffusion (A1111)",
      "http://127.0.0.1:7860",
      false },
    { AiProviderType::FalAi,
      "fal.ai",
      "https://queue.fal.run",
      true },
    { AiProviderType::Replicate,
      "Replicate",
      "https://api.replicate.com/v1",
      true },
    { AiProviderType::StabilityAi,
      "Stability AI",
      "https://api.stability.ai/v1",
      true },
    { AiProviderType::Custom,
      "Custom API",
      "",
      false },
  };
}

// Returns a catalog of well-known models for a given provider type.
// For local providers, this is populated dynamically at runtime.
inline std::vector<AiModelInfo> getDefaultModels(AiProviderType type) {
  std::vector<AiModelInfo> models;

  switch (type) {
    case AiProviderType::FalAi: {
      // Text-to-Image models
      {
        AiModelInfo m;
        m.id = "fal-ai/flux/schnell";
        m.displayName = "FLUX.1 Schnell";
        m.description = "Fast text-to-image generation";
        m.supportsTextToImage = true;
        models.push_back(m);
      }
      {
        AiModelInfo m;
        m.id = "fal-ai/stable-diffusion-v35-large";
        m.displayName = "Stable Diffusion 3.5 Large";
        m.description = "High quality text-to-image";
        m.supportsTextToImage = true;
        m.supportsImageToImage = true;
        models.push_back(m);
      }
      // Animation
      {
        AiModelInfo m;
        m.id = "fal-ai/animate-diff";
        m.displayName = "AnimateDiff";
        m.description = "Generate animations from text or image";
        m.supportsTextToImage = true;
        m.supportsImageToImage = true;
        m.supportsAnimation = true;
        models.push_back(m);
      }
      break;
    }

    case AiProviderType::Replicate: {
      {
        AiModelInfo m;
        m.id = "stability-ai/sdxl";
        m.displayName = "SDXL";
        m.description = "Stable Diffusion XL for high quality images";
        m.supportsTextToImage = true;
        m.supportsImageToImage = true;
        models.push_back(m);
      }
      break;
    }

    case AiProviderType::StabilityAi: {
      {
        AiModelInfo m;
        m.id = "stable-diffusion-xl-1024-v1-0";
        m.displayName = "SDXL 1.0";
        m.description = "Stable Diffusion XL 1.0";
        m.supportsTextToImage = true;
        m.supportsImageToImage = true;
        models.push_back(m);
      }
      break;
    }

    case AiProviderType::LocalComfyUI:
    case AiProviderType::LocalAutomatic1111:
      // Models will be fetched dynamically at runtime
      break;

    case AiProviderType::Custom:
      // User defines everything manually
      break;
  }

  return models;
}

} // namespace net

#endif // NET_AI_PROVIDER_H_INCLUDED
