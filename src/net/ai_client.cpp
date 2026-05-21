// MonoSprite AI Client - API Communication Layer
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "net/ai_client.h"
#include "net/ai_config.h"
#include "net/http_request.h"
#include "net/http_response.h"

#include "base/base64.h"
#include "base/log.h"

#include "json/json.hpp"

#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace net {

AiClient::AiClient()
  : m_config(nullptr)
  , m_busy(false)
  , m_cancelRequested(false)
{
}

AiClient::~AiClient()
{
  cancelAll();
}

void AiClient::setConfig(AiConfig* config)
{
  m_config = config;
}

bool AiClient::isBusy() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_busy;
}

void AiClient::cancelAll()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  m_cancelRequested = true;
}

// ---------------------------------------------------------------------------
// Core API Methods
// ---------------------------------------------------------------------------

void AiClient::textToImage(const std::string& prompt,
                           int width, int height,
                           AiCallback callback)
{
  if (!m_config) {
    AiResponse err;
    err.status = AiStatus::NetworkError;
    err.errorMessage = "AI client not configured. Please select a provider.";
    callback(err);
    return;
  }

  std::string fullPrompt = augmentPrompt(prompt);
  std::string payload = buildTextToImagePayload(fullPrompt, width, height);
  std::string url = buildEndpointUrl("text-to-image");

  executeAsync(url, "POST", payload, callback);
}

void AiClient::imageToImage(const std::string& imagePngBase64,
                            const std::string& prompt,
                            double strength,
                            AiCallback callback)
{
  if (!m_config) {
    AiResponse err;
    err.status = AiStatus::NetworkError;
    err.errorMessage = "AI client not configured. Please select a provider.";
    callback(err);
    return;
  }

  std::string fullPrompt = augmentPrompt(prompt);
  std::string payload = buildImageToImagePayload(imagePngBase64, fullPrompt, strength);
  std::string url = buildEndpointUrl("image-to-image");

  executeAsync(url, "POST", payload, callback);
}

void AiClient::fetchLocalModels(AiCallback callback)
{
  if (!m_config) {
    AiResponse err;
    err.status = AiStatus::NetworkError;
    err.errorMessage = "AI client not configured.";
    callback(err);
    return;
  }

  std::string url;
  AiProviderType type = m_config->providerType();

  if (type == AiProviderType::LocalComfyUI) {
    // ComfyUI: GET /object_info to list available nodes and models
    url = m_config->baseUrl() + "/object_info";
  }
  else if (type == AiProviderType::LocalAutomatic1111) {
    // A1111: GET /sdapi/v1/sd-models to list checkpoints
    url = m_config->baseUrl() + "/sdapi/v1/sd-models";
  }
  else {
    AiResponse err;
    err.status = AiStatus::NetworkError;
    err.errorMessage = "Fetching models is only supported for local providers.";
    callback(err);
    return;
  }

  executeAsync(url, "GET", "", callback);
}

void AiClient::testConnection(AiCallback callback)
{
  if (!m_config) {
    AiResponse err;
    err.status = AiStatus::NetworkError;
    err.errorMessage = "AI client not configured.";
    callback(err);
    return;
  }

  std::string url;
  AiProviderType type = m_config->providerType();

  switch (type) {
    case AiProviderType::LocalComfyUI:
      url = m_config->baseUrl() + "/system_stats";
      break;
    case AiProviderType::LocalAutomatic1111:
      url = m_config->baseUrl() + "/sdapi/v1/options";
      break;
    case AiProviderType::FalAi:
    case AiProviderType::Replicate:
    case AiProviderType::StabilityAi:
    case AiProviderType::Custom:
      // For cloud APIs, just try a lightweight request
      url = m_config->baseUrl();
      break;
  }

  executeAsync(url, "GET", "", callback);
}

// ---------------------------------------------------------------------------
// Internal: Prompt Augmentation
// ---------------------------------------------------------------------------

std::string AiClient::augmentPrompt(const std::string& userPrompt) const
{
  if (!m_config || !m_config->autoPixelPrompt())
    return userPrompt;

  // Append pixel art style keywords to guide the model output
  static const std::string pixelSuffix =
    ", pixel art, 8-bit style, 16-bit, clean outline, "
    "hand-drawn pixels, crisp edges, no anti-aliasing";

  // Check if user already specified pixel art keywords
  std::string lowerPrompt = userPrompt;
  std::transform(lowerPrompt.begin(), lowerPrompt.end(),
                 lowerPrompt.begin(), ::tolower);

  if (lowerPrompt.find("pixel art") != std::string::npos ||
      lowerPrompt.find("pixel-art") != std::string::npos ||
      lowerPrompt.find("8-bit") != std::string::npos ||
      lowerPrompt.find("16-bit") != std::string::npos) {
    return userPrompt; // User already specified pixel art style
  }

  return userPrompt + pixelSuffix;
}

// ---------------------------------------------------------------------------
// Internal: Build Provider-Specific Payloads
// ---------------------------------------------------------------------------

std::string AiClient::buildTextToImagePayload(const std::string& prompt,
                                              int width, int height) const
{
  json::Value payload = json::Value::makeObject();
  AiProviderType type = m_config->providerType();

  switch (type) {
    case AiProviderType::FalAi: {
      payload["prompt"] = json::Value(prompt);
      payload["image_size"] = json::Value::makeObject();
      payload["image_size"]["width"] = json::Value(width);
      payload["image_size"]["height"] = json::Value(height);
      payload["num_images"] = json::Value(1);
      break;
    }

    case AiProviderType::Replicate: {
      payload["input"] = json::Value::makeObject();
      payload["input"]["prompt"] = json::Value(prompt);
      payload["input"]["width"] = json::Value(width);
      payload["input"]["height"] = json::Value(height);
      break;
    }

    case AiProviderType::StabilityAi: {
      json::Value textPrompts = json::Value::makeArray();
      json::Value textPrompt = json::Value::makeObject();
      textPrompt["text"] = json::Value(prompt);
      textPrompt["weight"] = json::Value(1.0);
      textPrompts.push_back(textPrompt);
      payload["text_prompts"] = textPrompts;
      payload["width"] = json::Value(width);
      payload["height"] = json::Value(height);
      payload["samples"] = json::Value(1);
      break;
    }

    case AiProviderType::LocalAutomatic1111: {
      payload["prompt"] = json::Value(prompt);
      payload["width"] = json::Value(width);
      payload["height"] = json::Value(height);
      payload["steps"] = json::Value(20);
      payload["batch_size"] = json::Value(1);
      break;
    }

    case AiProviderType::LocalComfyUI: {
      // ComfyUI uses a workflow-based API; this is a simplified payload.
      // In practice, a full ComfyUI workflow JSON would be used.
      payload["prompt"] = json::Value(prompt);
      payload["width"] = json::Value(width);
      payload["height"] = json::Value(height);
      break;
    }

    case AiProviderType::Custom: {
      payload["prompt"] = json::Value(prompt);
      payload["width"] = json::Value(width);
      payload["height"] = json::Value(height);
      break;
    }
  }

  return payload.serialize();
}

std::string AiClient::buildImageToImagePayload(const std::string& imagePngBase64,
                                               const std::string& prompt,
                                               double strength) const
{
  json::Value payload = json::Value::makeObject();
  AiProviderType type = m_config->providerType();

  switch (type) {
    case AiProviderType::FalAi: {
      payload["prompt"] = json::Value(prompt);
      payload["image_url"] = json::Value(
        std::string("data:image/png;base64,") + imagePngBase64);
      payload["strength"] = json::Value(strength);
      payload["num_images"] = json::Value(1);
      break;
    }

    case AiProviderType::Replicate: {
      payload["input"] = json::Value::makeObject();
      payload["input"]["prompt"] = json::Value(prompt);
      payload["input"]["image"] = json::Value(
        std::string("data:image/png;base64,") + imagePngBase64);
      payload["input"]["prompt_strength"] = json::Value(strength);
      break;
    }

    case AiProviderType::StabilityAi: {
      json::Value textPrompts = json::Value::makeArray();
      json::Value textPrompt = json::Value::makeObject();
      textPrompt["text"] = json::Value(prompt);
      textPrompt["weight"] = json::Value(1.0);
      textPrompts.push_back(textPrompt);
      payload["text_prompts"] = textPrompts;
      payload["init_image"] = json::Value(imagePngBase64);
      payload["image_strength"] = json::Value(strength);
      break;
    }

    case AiProviderType::LocalAutomatic1111: {
      payload["prompt"] = json::Value(prompt);
      json::Value initImages = json::Value::makeArray();
      initImages.push_back(json::Value(imagePngBase64));
      payload["init_images"] = initImages;
      payload["denoising_strength"] = json::Value(strength);
      payload["steps"] = json::Value(20);
      break;
    }

    case AiProviderType::LocalComfyUI:
    case AiProviderType::Custom: {
      payload["prompt"] = json::Value(prompt);
      payload["image"] = json::Value(imagePngBase64);
      payload["strength"] = json::Value(strength);
      break;
    }
  }

  return payload.serialize();
}

// ---------------------------------------------------------------------------
// Internal: Auth Headers
// ---------------------------------------------------------------------------

std::unordered_map<std::string, std::string> AiClient::buildHeaders() const
{
  std::unordered_map<std::string, std::string> headers;
  headers["Content-Type"] = "application/json";
  headers["Accept"] = "application/json";

  if (!m_config)
    return headers;

  AiProviderType type = m_config->providerType();
  const std::string& apiKey = m_config->apiKey();

  if (apiKey.empty())
    return headers;

  switch (type) {
    case AiProviderType::FalAi:
      headers["Authorization"] = "Key " + apiKey;
      break;
    case AiProviderType::Replicate:
      headers["Authorization"] = "Bearer " + apiKey;
      break;
    case AiProviderType::StabilityAi:
      headers["Authorization"] = "Bearer " + apiKey;
      break;
    case AiProviderType::Custom:
      headers["Authorization"] = "Bearer " + apiKey;
      break;
    default:
      // Local providers don't need auth headers
      break;
  }

  return headers;
}

// ---------------------------------------------------------------------------
// Internal: Endpoint URL
// ---------------------------------------------------------------------------

std::string AiClient::buildEndpointUrl(const std::string& action) const
{
  if (!m_config)
    return "";

  AiProviderType type = m_config->providerType();
  std::string base = m_config->baseUrl();
  std::string modelId = m_config->selectedModelId();

  // Remove trailing slash from base URL
  while (!base.empty() && base.back() == '/')
    base.pop_back();

  switch (type) {
    case AiProviderType::FalAi: {
      // fal.ai format: https://queue.fal.run/{model_id}
      if (!modelId.empty())
        return base + "/" + modelId;
      return base + "/fal-ai/flux/schnell"; // default fallback
    }

    case AiProviderType::Replicate: {
      // Replicate format: https://api.replicate.com/v1/predictions
      return base + "/predictions";
    }

    case AiProviderType::StabilityAi: {
      // Stability AI format: https://api.stability.ai/v1/generation/{engine_id}/text-to-image
      std::string engine = modelId.empty()
        ? "stable-diffusion-xl-1024-v1-0" : modelId;
      return base + "/generation/" + engine + "/" + action;
    }

    case AiProviderType::LocalAutomatic1111: {
      if (action == "text-to-image")
        return base + "/sdapi/v1/txt2img";
      else if (action == "image-to-image")
        return base + "/sdapi/v1/img2img";
      return base;
    }

    case AiProviderType::LocalComfyUI: {
      // ComfyUI uses /prompt endpoint for all generation
      return base + "/prompt";
    }

    case AiProviderType::Custom: {
      return base;
    }
  }

  return base;
}

// ---------------------------------------------------------------------------
// Internal: Async Execution
// ---------------------------------------------------------------------------

void AiClient::executeAsync(const std::string& url,
                            const std::string& method,
                            const std::string& body,
                            AiCallback callback)
{
  // Prevent concurrent requests
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_busy) {
      AiResponse err;
      err.status = AiStatus::NetworkError;
      err.errorMessage = "Another request is already in progress. Please wait.";
      callback(err);
      return;
    }
    m_busy = true;
    m_cancelRequested = false;
  }

  auto headers = buildHeaders();

  // Capture by value for the thread
  auto capturedConfig = m_config;
  auto self = this;

  std::thread([self, url, method, body, headers, callback]() {
    LOG(INFO) << "AI Client: " << method << " " << url << "\n";

    std::stringstream responseStream;
    HttpRequest request(url);

    // Set headers
    HttpHeaders httpHeaders;
    for (auto& kv : headers) {
      httpHeaders[kv.first] = kv.second;
    }
    request.setHeaders(httpHeaders);

    // Set body for POST requests
    if (method == "POST" && !body.empty()) {
      request.setPostBody(body);
    }

    // Send the request
    HttpResponse response(&responseStream);
    bool success = request.send(response);

    // Check if cancelled
    {
      std::lock_guard<std::mutex> lock(self->m_mutex);
      if (self->m_cancelRequested) {
        self->m_busy = false;
        AiResponse cancelled;
        cancelled.status = AiStatus::Cancelled;
        cancelled.errorMessage = "Request cancelled.";
        callback(cancelled);
        return;
      }
    }

    // Parse the response
    AiResponse aiResponse;
    if (!success) {
      aiResponse.status = AiStatus::NetworkError;
      aiResponse.errorMessage =
        "Network error: Could not connect to " + url + ". "
        "Please check that the server is running and the URL is correct.";
    }
    else {
      aiResponse = self->parseResponse(response.status(),
                                       responseStream.str());
    }

    // Mark as no longer busy
    {
      std::lock_guard<std::mutex> lock(self->m_mutex);
      self->m_busy = false;
    }

    // Deliver the result via callback
    callback(aiResponse);

  }).detach();
}

// ---------------------------------------------------------------------------
// Internal: Response Parsing
// ---------------------------------------------------------------------------

AiResponse AiClient::parseResponse(int httpStatus,
                                   const std::string& responseBody) const
{
  AiResponse response;
  response.httpStatusCode = httpStatus;

  LOG(INFO) << "AI Client: HTTP " << httpStatus
            << " (" << responseBody.size() << " bytes)\n";

  // Map HTTP status code to AiStatus
  if (httpStatus >= 200 && httpStatus < 300) {
    response.status = AiStatus::Success;
  }
  else if (httpStatus == 401) {
    response.status = AiStatus::InvalidApiKey;
    response.errorMessage =
      "Authentication failed (HTTP 401). "
      "Please check your API key in the Settings panel.";
    return response;
  }
  else if (httpStatus == 402) {
    response.status = AiStatus::InsufficientFunds;
    response.errorMessage =
      "Insufficient funds (HTTP 402). "
      "Your API account has run out of credits. "
      "Please add funds or switch to a different provider.";
    return response;
  }
  else if (httpStatus == 403) {
    response.status = AiStatus::Forbidden;
    response.errorMessage =
      "Access denied (HTTP 403). "
      "Your API key does not have permission for this model.";
    return response;
  }
  else if (httpStatus == 404) {
    response.status = AiStatus::NotFound;
    response.errorMessage =
      "Not found (HTTP 404). "
      "The model or endpoint does not exist. "
      "Please check your model selection.";
    return response;
  }
  else if (httpStatus == 429) {
    response.status = AiStatus::RateLimited;
    response.errorMessage =
      "Rate limit reached (HTTP 429). "
      "Too many requests. Please wait before trying again.";

    // Try to extract retry-after info from response body
    try {
      json::Value parsed = json::Value::parse(responseBody);
      if (parsed.hasKey("retry_after")) {
        response.retryAfterSeconds = parsed["retry_after"].asInt();
      } else {
        response.retryAfterSeconds = 30; // Default: wait 30 seconds
      }
    }
    catch (...) {
      response.retryAfterSeconds = 30;
    }

    if (response.retryAfterSeconds > 0) {
      response.errorMessage +=
        " Retry in " + std::to_string(response.retryAfterSeconds) + " seconds.";
    }
    return response;
  }
  else if (httpStatus >= 500) {
    response.status = AiStatus::ServerError;
    response.errorMessage =
      "Server error (HTTP " + std::to_string(httpStatus) + "). "
      "The AI provider is experiencing issues. Please try again later.";
    return response;
  }
  else if (httpStatus == 0) {
    response.status = AiStatus::NetworkError;
    response.errorMessage =
      "Network error: No response received. "
      "Please check your internet connection and server URL.";
    return response;
  }
  else {
    response.status = AiStatus::ServerError;
    response.errorMessage =
      "Unexpected HTTP status " + std::to_string(httpStatus) + ".";
    return response;
  }

  // Parse successful response body
  response.data = responseBody;

  if (responseBody.empty())
    return response;

  try {
    json::Value parsed = json::Value::parse(responseBody);
    AiProviderType type = m_config ? m_config->providerType()
                                   : AiProviderType::Custom;

    switch (type) {
      case AiProviderType::FalAi: {
        // fal.ai returns: { "images": [{"url": "..."}, ...] }
        if (parsed.hasKey("images") && parsed["images"].size() > 0) {
          const auto& images = parsed["images"];
          for (size_t i = 0; i < images.size(); ++i) {
            if (images[i].hasKey("url")) {
              response.imageUrl = images[i]["url"].asString();
            }
            if (images[i].hasKey("content")) {
              response.imagesBase64.push_back(
                images[i]["content"].asString());
            }
          }
          if (!response.imagesBase64.empty()) {
            response.imageBase64 = response.imagesBase64[0];
          }
        }
        break;
      }

      case AiProviderType::Replicate: {
        // Replicate returns: { "output": ["url1", ...] }
        // or { "status": "processing", "urls": {"get": "..."} }
        if (parsed.hasKey("output") && parsed["output"].size() > 0) {
          response.imageUrl = parsed["output"][0].asString();
        }
        else if (parsed.hasKey("status")) {
          std::string status = parsed["status"].asString();
          if (status == "processing" || status == "starting") {
            response.status = AiStatus::Pending;
            response.errorMessage = "Generation in progress...";
          }
        }
        break;
      }

      case AiProviderType::StabilityAi: {
        // Stability AI returns: { "artifacts": [{"base64": "..."}, ...] }
        if (parsed.hasKey("artifacts") && parsed["artifacts"].size() > 0) {
          const auto& artifacts = parsed["artifacts"];
          for (size_t i = 0; i < artifacts.size(); ++i) {
            if (artifacts[i].hasKey("base64")) {
              response.imagesBase64.push_back(
                artifacts[i]["base64"].asString());
            }
          }
          if (!response.imagesBase64.empty()) {
            response.imageBase64 = response.imagesBase64[0];
          }
        }
        break;
      }

      case AiProviderType::LocalAutomatic1111: {
        // A1111 returns: { "images": ["base64string", ...] }
        if (parsed.hasKey("images") && parsed["images"].size() > 0) {
          for (size_t i = 0; i < parsed["images"].size(); ++i) {
            response.imagesBase64.push_back(
              parsed["images"][i].asString());
          }
          response.imageBase64 = response.imagesBase64[0];
        }
        break;
      }

      case AiProviderType::LocalComfyUI: {
        // ComfyUI: prompt_id is returned, images need separate retrieval
        if (parsed.hasKey("prompt_id")) {
          response.data = parsed["prompt_id"].asString();
        }
        break;
      }

      case AiProviderType::Custom: {
        // Try common response formats
        if (parsed.hasKey("images") && parsed["images"].size() > 0) {
          for (size_t i = 0; i < parsed["images"].size(); ++i) {
            const auto& img = parsed["images"][i];
            if (img.isString()) {
              response.imagesBase64.push_back(img.asString());
            } else if (img.hasKey("base64")) {
              response.imagesBase64.push_back(img["base64"].asString());
            } else if (img.hasKey("url")) {
              response.imageUrl = img["url"].asString();
            }
          }
          if (!response.imagesBase64.empty()) {
            response.imageBase64 = response.imagesBase64[0];
          }
        }
        else if (parsed.hasKey("image")) {
          response.imageBase64 = parsed["image"].asString();
          response.imagesBase64.push_back(response.imageBase64);
        }
        break;
      }
    }

    // Check for error messages in JSON response (some APIs return 200 with errors)
    if (parsed.hasKey("error")) {
      if (parsed["error"].isString()) {
        response.status = AiStatus::ServerError;
        response.errorMessage = parsed["error"].asString();
      }
      else if (parsed["error"].hasKey("message")) {
        response.status = AiStatus::ServerError;
        response.errorMessage = parsed["error"]["message"].asString();
      }
    }
  }
  catch (...) {
    // If JSON parsing fails but HTTP was OK, the response might
    // be raw binary image data. Keep it in response.data.
    LOG(WARNING) << "AI Client: Failed to parse JSON response\n";
  }

  return response;
}

void AiClient::parseRateLimitInfo(const std::string& headers,
                                  AiResponse& response) const
{
  // Parse standard rate limit headers
  // Retry-After: 30
  // X-RateLimit-Remaining: 0
  // X-RateLimit-Reset: 1621500000

  std::istringstream stream(headers);
  std::string line;

  while (std::getline(stream, line)) {
    // Remove trailing \r
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    // Convert header name to lowercase for case-insensitive matching
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos) continue;

    std::string headerName = line.substr(0, colonPos);
    std::string headerValue = line.substr(colonPos + 1);

    // Trim whitespace
    while (!headerValue.empty() && headerValue.front() == ' ')
      headerValue.erase(headerValue.begin());

    std::transform(headerName.begin(), headerName.end(),
                   headerName.begin(), ::tolower);

    if (headerName == "retry-after") {
      response.retryAfterSeconds = std::stoi(headerValue);
    }
    else if (headerName == "x-ratelimit-remaining") {
      response.rateLimitRemaining = std::stoi(headerValue);
    }
    else if (headerName == "x-ratelimit-reset") {
      response.rateLimitResetTimestamp = std::stoi(headerValue);
    }
  }
}

} // namespace net
