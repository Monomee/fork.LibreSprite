// MonoSprite AI Client - API Communication Layer
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef NET_AI_CLIENT_H_INCLUDED
#define NET_AI_CLIENT_H_INCLUDED
#pragma once

#include "net/ai_provider.h"

#include <functional>
#include <string>
#include <vector>
#include <mutex>

namespace net {

class AiConfig;

// Status of an AI API call.
// These map closely to HTTP status codes but are simplified
// for the UI to show appropriate feedback to the user.
enum class AiStatus {
  Success,            // 200 OK - Request completed successfully
  Pending,            // Request is in progress (async)
  InvalidApiKey,      // 401 Unauthorized - Bad or missing API key
  InsufficientFunds,  // 402 Payment Required - Account out of credits
  Forbidden,          // 403 Forbidden - API key lacks permissions
  NotFound,           // 404 Not Found - Model or endpoint not found
  RateLimited,        // 429 Too Many Requests - Hit rate limit
  ServerError,        // 500+ Server Error - Provider-side failure
  NetworkError,       // Connection failed, DNS error, timeout, etc.
  ParseError,         // Response was not valid JSON
  Cancelled           // Request was cancelled by the user
};

// Encapsulates the result of an AI API call.
struct AiResponse {
  AiStatus status;
  int httpStatusCode;

  // On success: the generated image data (PNG bytes) or JSON text
  std::string data;

  // On success with image: base64-encoded PNG string from the API
  std::string imageBase64;

  // On success with multiple images: list of base64-encoded PNG strings
  std::vector<std::string> imagesBase64;

  // URL to the generated image (if the API returns a URL instead of base64)
  std::string imageUrl;

  // Error message to display in the sidebar chat
  std::string errorMessage;

  // Rate limit info (populated when status == RateLimited)
  int retryAfterSeconds;       // Seconds to wait before retrying
  int rateLimitRemaining;      // Remaining requests in current window
  int rateLimitResetTimestamp;  // Unix timestamp when limit resets

  AiResponse()
    : status(AiStatus::Pending)
    , httpStatusCode(0)
    , retryAfterSeconds(0)
    , rateLimitRemaining(-1)
    , rateLimitResetTimestamp(0) {}
};

// Callback type for asynchronous AI requests.
// Called on a background thread - the UI must marshal to the main thread.
using AiCallback = std::function<void(const AiResponse&)>;

// Main AI client class.
//
// This class handles all communication with AI provider APIs.
// Requests are executed asynchronously on background threads.
// Results are delivered via callbacks.
//
// Usage:
//   net::AiClient client;
//   client.setConfig(&myAiConfig);
//   client.textToImage("cute slime pixel art", callback);
//
class AiClient {
  friend class AiClientTest;
public:
  AiClient();
  ~AiClient();

  // Set the configuration to use for API calls.
  // The config is not owned by this class.
  void setConfig(AiConfig* config);

  // --- Core API Methods ---

  // Generate an image from a text prompt.
  // The callback will be called on a background thread when complete.
  void textToImage(const std::string& prompt,
                   int width, int height,
                   AiCallback callback);

  // Transform an existing image using a text prompt (img2img).
  // imagePngBase64: the source image encoded as base64 PNG.
  void imageToImage(const std::string& imagePngBase64,
                    const std::string& prompt,
                    double strength,
                    AiCallback callback);

  // Fetch available models from a local server (ComfyUI / A1111).
  // On success, response.data contains JSON array of model info.
  void fetchLocalModels(AiCallback callback);

  // Test connection to the configured provider.
  // Useful for validating API key and base URL.
  void testConnection(AiCallback callback);

  // Cancel all pending requests.
  void cancelAll();

  // Returns true if there is a request currently in progress.
  bool isBusy() const;

private:
  // Internal: Build provider-specific request payloads
  std::string buildTextToImagePayload(const std::string& prompt,
                                      int width, int height) const;

  std::string buildImageToImagePayload(const std::string& imagePngBase64,
                                       const std::string& prompt,
                                       double strength) const;

  // Internal: Execute an HTTP request on a background thread
  void executeAsync(const std::string& url,
                    const std::string& method,
                    const std::string& body,
                    AiCallback callback);

  // Internal: Parse the HTTP response into an AiResponse
  AiResponse parseResponse(int httpStatus,
                            const std::string& responseBody) const;

  // Internal: Parse rate limit headers from response
  void parseRateLimitInfo(const std::string& headers,
                          AiResponse& response) const;

  // Internal: Augment user prompt with pixel art keywords if enabled
  std::string augmentPrompt(const std::string& userPrompt) const;

  // Internal: Build auth headers for the current provider
  std::unordered_map<std::string, std::string> buildHeaders() const;

  // Internal: Build the full API endpoint URL for a given action
  std::string buildEndpointUrl(const std::string& action) const;

  AiConfig* m_config;
  mutable std::mutex m_mutex;
  bool m_busy;
  bool m_cancelRequested;
};

} // namespace net

#endif // NET_AI_CLIENT_H_INCLUDED
