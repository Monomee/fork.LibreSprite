// MonoSprite AI Client Tests
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include <gtest/gtest.h>
#include "net/ai_provider.h"
#include "net/ai_config.h"
#include "net/ai_client.h"
#include "json/json.hpp"

#include <map>
#include <string>

// Mock implementation of app::ini_file functions to avoid linking app-lib
namespace app {
  static std::map<std::string, std::string> mock_config;

  void flush_config_file() {}

  const char* get_config_string(const char* section, const char* name, const char* value) {
    std::string key = std::string(section) + "/" + name;
    auto it = mock_config.find(key);
    if (it != mock_config.end()) {
      return it->second.c_str();
    }
    return value;
  }

  void set_config_string(const char* section, const char* name, const char* value) {
    std::string key = std::string(section) + "/" + name;
    mock_config[key] = value ? value : "";
  }

  int get_config_int(const char* section, const char* name, int value) {
    std::string key = std::string(section) + "/" + name;
    auto it = mock_config.find(key);
    if (it != mock_config.end()) {
      return std::stoi(it->second);
    }
    return value;
  }

  void set_config_int(const char* section, const char* name, int value) {
    std::string key = std::string(section) + "/" + name;
    mock_config[key] = std::to_string(value);
  }

  bool get_config_bool(const char* section, const char* name, bool value) {
    std::string key = std::string(section) + "/" + name;
    auto it = mock_config.find(key);
    if (it != mock_config.end()) {
      return it->second == "true" || it->second == "1";
    }
    return value;
  }

  void set_config_bool(const char* section, const char* name, bool value) {
    std::string key = std::string(section) + "/" + name;
    mock_config[key] = value ? "true" : "false";
  }
}

// Mock she::log to avoid linking she library
namespace she {
  void log(const std::string& msg) {
    // No-op for unit tests
  }
}

namespace net {

// Test fixture that makes it easy to test private methods of AiClient
class AiClientTest : public ::testing::Test {
protected:
  AiClient client;
  AiConfig config;

  void SetUp() override {
    app::mock_config.clear();
    client.setConfig(&config);
  }

  std::string callAugmentPrompt(const std::string& prompt) {
    return client.augmentPrompt(prompt);
  }

  std::unordered_map<std::string, std::string> callBuildHeaders() {
    return client.buildHeaders();
  }

  std::string callBuildEndpointUrl(const std::string& action) {
    return client.buildEndpointUrl(action);
  }

  std::string callBuildTextToImagePayload(const std::string& prompt, int w, int h) {
    return client.buildTextToImagePayload(prompt, w, h);
  }

  std::string callBuildImageToImagePayload(const std::string& b64, const std::string& prompt, double strength) {
    return client.buildImageToImagePayload(b64, prompt, strength);
  }

  AiResponse callParseResponse(int status, const std::string& body) {
    return client.parseResponse(status, body);
  }

  void callParseRateLimitInfo(const std::string& headers, AiResponse& resp) {
    client.parseRateLimitInfo(headers, resp);
  }
};

// --- AiConfig Tests ---

TEST(AiConfig, DefaultValues)
{
  app::mock_config.clear();
  AiConfig cfg;
  cfg.load();

  EXPECT_EQ(AiProviderType::LocalComfyUI, cfg.providerType());
  EXPECT_EQ("http://127.0.0.1:8188", cfg.baseUrl());
  EXPECT_EQ("", cfg.apiKey());
  EXPECT_EQ("", cfg.selectedModelId());
  EXPECT_TRUE(cfg.sidebarVisible());
  EXPECT_TRUE(cfg.autoPixelPrompt());
}

TEST(AiConfig, SaveAndLoad)
{
  app::mock_config.clear();
  AiConfig cfg;
  cfg.setProviderType(AiProviderType::FalAi);
  cfg.setBaseUrl("https://custom.fal.run");
  cfg.setApiKey("test-key-123");
  cfg.setSelectedModelId("my-special-model");
  cfg.setSidebarVisible(false);
  cfg.setAutoPixelPrompt(false);
  cfg.save();

  AiConfig cfg2;
  cfg2.load();
  EXPECT_EQ(AiProviderType::FalAi, cfg2.providerType());
  EXPECT_EQ("https://custom.fal.run", cfg2.baseUrl());
  EXPECT_EQ("test-key-123", cfg2.apiKey());
  EXPECT_EQ("my-special-model", cfg2.selectedModelId());
  EXPECT_FALSE(cfg2.sidebarVisible());
  EXPECT_FALSE(cfg2.autoPixelPrompt());
}

TEST(AiConfig, ProviderDefaultsReset)
{
  app::mock_config.clear();
  AiConfig cfg;
  cfg.setProviderType(AiProviderType::FalAi);
  EXPECT_EQ("https://queue.fal.run", cfg.baseUrl());

  cfg.setProviderType(AiProviderType::LocalAutomatic1111);
  EXPECT_EQ("http://127.0.0.1:7860", cfg.baseUrl());
}

// --- AiClient Helper Tests ---

TEST_F(AiClientTest, PromptAugmentation)
{
  // With autoPixelPrompt enabled (default)
  config.setAutoPixelPrompt(true);
  std::string augmented = callAugmentPrompt("a red dragon");
  EXPECT_NE(std::string::npos, augmented.find("pixel art"));
  EXPECT_NE(std::string::npos, augmented.find("8-bit style"));

  // Already contains pixel art keywords -> no change
  std::string noChange1 = callAugmentPrompt("pixel art of a cat");
  EXPECT_EQ("pixel art of a cat", noChange1);

  std::string noChange2 = callAugmentPrompt("8-bit retro hero");
  EXPECT_EQ("8-bit retro hero", noChange2);

  // With autoPixelPrompt disabled
  config.setAutoPixelPrompt(false);
  std::string direct = callAugmentPrompt("a red dragon");
  EXPECT_EQ("a red dragon", direct);
}

TEST_F(AiClientTest, HeadersGeneration)
{
  // Local provider -> no auth header
  config.setProviderType(AiProviderType::LocalComfyUI);
  config.setApiKey("unused-key");
  auto headers = callBuildHeaders();
  EXPECT_EQ(0, headers.count("Authorization"));

  // Cloud provider FalAi -> Authorization: Key ...
  config.setProviderType(AiProviderType::FalAi);
  config.setApiKey("fal-secret-xyz");
  headers = callBuildHeaders();
  EXPECT_EQ("Key fal-secret-xyz", headers["Authorization"]);

  // Cloud provider Replicate -> Authorization: Bearer ...
  config.setProviderType(AiProviderType::Replicate);
  config.setApiKey("replicate-secret-xyz");
  headers = callBuildHeaders();
  EXPECT_EQ("Bearer replicate-secret-xyz", headers["Authorization"]);

  // Cloud provider Stability AI -> Authorization: Bearer ...
  config.setProviderType(AiProviderType::StabilityAi);
  config.setApiKey("stability-secret-xyz");
  headers = callBuildHeaders();
  EXPECT_EQ("Bearer stability-secret-xyz", headers["Authorization"]);
}

TEST_F(AiClientTest, EndpointUrlGeneration)
{
  // FalAi
  config.setProviderType(AiProviderType::FalAi);
  config.setBaseUrl("https://queue.fal.run/");
  config.setSelectedModelId("fal-ai/flux/schnell");
  EXPECT_EQ("https://queue.fal.run/fal-ai/flux/schnell", callBuildEndpointUrl("text-to-image"));

  // Replicate
  config.setProviderType(AiProviderType::Replicate);
  config.setBaseUrl("https://api.replicate.com/v1");
  EXPECT_EQ("https://api.replicate.com/v1/predictions", callBuildEndpointUrl("text-to-image"));

  // Stability AI
  config.setProviderType(AiProviderType::StabilityAi);
  config.setBaseUrl("https://api.stability.ai/v1");
  config.setSelectedModelId("stable-diffusion-xl-1024-v1-0");
  EXPECT_EQ("https://api.stability.ai/v1/generation/stable-diffusion-xl-1024-v1-0/text-to-image",
            callBuildEndpointUrl("text-to-image"));

  // Local A1111
  config.setProviderType(AiProviderType::LocalAutomatic1111);
  config.setBaseUrl("http://127.0.0.1:7860");
  EXPECT_EQ("http://127.0.0.1:7860/sdapi/v1/txt2img", callBuildEndpointUrl("text-to-image"));
  EXPECT_EQ("http://127.0.0.1:7860/sdapi/v1/img2img", callBuildEndpointUrl("image-to-image"));

  // Local ComfyUI
  config.setProviderType(AiProviderType::LocalComfyUI);
  config.setBaseUrl("http://127.0.0.1:8188");
  EXPECT_EQ("http://127.0.0.1:8188/prompt", callBuildEndpointUrl("text-to-image"));
}

TEST_F(AiClientTest, PayloadGeneration)
{
  // FalAi T2I payload
  config.setProviderType(AiProviderType::FalAi);
  std::string payloadStr = callBuildTextToImagePayload("cute slime", 256, 256);
  json::Value payload = json::Value::parse(payloadStr);
  EXPECT_EQ("cute slime", payload["prompt"].asString());
  EXPECT_EQ(256, payload["image_size"]["width"].asInt());
  EXPECT_EQ(256, payload["image_size"]["height"].asInt());

  // A1111 T2I payload
  config.setProviderType(AiProviderType::LocalAutomatic1111);
  payloadStr = callBuildTextToImagePayload("cute slime", 128, 128);
  payload = json::Value::parse(payloadStr);
  EXPECT_EQ("cute slime", payload["prompt"].asString());
  EXPECT_EQ(128, payload["width"].asInt());
  EXPECT_EQ(20, payload["steps"].asInt());

  // FalAi I2I payload
  config.setProviderType(AiProviderType::FalAi);
  payloadStr = callBuildImageToImagePayload("i_am_base64", "pixel hero", 0.6);
  payload = json::Value::parse(payloadStr);
  EXPECT_EQ("pixel hero", payload["prompt"].asString());
  EXPECT_EQ("data:image/png;base64,i_am_base64", payload["image_url"].asString());
  EXPECT_DOUBLE_EQ(0.6, payload["strength"].asNumber());
}

TEST_F(AiClientTest, ParseErrorResponses)
{
  // 401 Unauthorized
  AiResponse r = callParseResponse(401, "{\"error\": \"Bad credentials\"}");
  EXPECT_EQ(AiStatus::InvalidApiKey, r.status);
  EXPECT_NE(std::string::npos, r.errorMessage.find("API key"));

  // 429 Rate limited
  r = callParseResponse(429, "{\"retry_after\": 15}");
  EXPECT_EQ(AiStatus::RateLimited, r.status);
  EXPECT_EQ(15, r.retryAfterSeconds);
  EXPECT_NE(std::string::npos, r.errorMessage.find("15 seconds"));

  // 500 Server error
  r = callParseResponse(500, "Internal Server Error");
  EXPECT_EQ(AiStatus::ServerError, r.status);
  EXPECT_NE(std::string::npos, r.errorMessage.find("AI provider is experiencing issues"));
}

TEST_F(AiClientTest, ParseSuccessResponses)
{
  // FalAi success parse
  config.setProviderType(AiProviderType::FalAi);
  std::string falResponse = "{\"images\": [{\"url\": \"https://fal.media/1.png\", \"content\": \"base64_data\"}]}";
  AiResponse r = callParseResponse(200, falResponse);
  EXPECT_EQ(AiStatus::Success, r.status);
  EXPECT_EQ("https://fal.media/1.png", r.imageUrl);
  EXPECT_EQ("base64_data", r.imageBase64);
  EXPECT_EQ(1, r.imagesBase64.size());

  // Replicate success parse
  config.setProviderType(AiProviderType::Replicate);
  std::string repResponse = "{\"output\": [\"https://replicate.delivery/out.png\"]}";
  r = callParseResponse(200, repResponse);
  EXPECT_EQ(AiStatus::Success, r.status);
  EXPECT_EQ("https://replicate.delivery/out.png", r.imageUrl);

  // Replicate processing status parse (returns Pending status)
  std::string repProcessing = "{\"status\": \"processing\"}";
  r = callParseResponse(200, repProcessing);
  EXPECT_EQ(AiStatus::Pending, r.status);

  // Stability AI success parse
  config.setProviderType(AiProviderType::StabilityAi);
  std::string stabResponse = "{\"artifacts\": [{\"base64\": \"stab_base64_data\"}]}";
  r = callParseResponse(200, stabResponse);
  EXPECT_EQ(AiStatus::Success, r.status);
  EXPECT_EQ("stab_base64_data", r.imageBase64);

  // A1111 success parse
  config.setProviderType(AiProviderType::LocalAutomatic1111);
  std::string a1111Response = "{\"images\": [\"a1111_b64\"]}";
  r = callParseResponse(200, a1111Response);
  EXPECT_EQ(AiStatus::Success, r.status);
  EXPECT_EQ("a1111_b64", r.imageBase64);
}

TEST_F(AiClientTest, ParseRateLimitHeaders)
{
  std::string headers =
    "HTTP/1.1 429 Too Many Requests\r\n"
    "Content-Type: application/json\r\n"
    "Retry-After: 45\r\n"
    "X-RateLimit-Remaining: 0\r\n"
    "X-RateLimit-Reset: 1716300000\r\n"
    "\r\n";

  AiResponse r;
  callParseRateLimitInfo(headers, r);
  EXPECT_EQ(45, r.retryAfterSeconds);
  EXPECT_EQ(0, r.rateLimitRemaining);
  EXPECT_EQ(1716300000, r.rateLimitResetTimestamp);
}

} // namespace net

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
