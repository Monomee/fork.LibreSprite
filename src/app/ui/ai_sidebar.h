// MonoSprite AI Extension
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#pragma once

#include "net/ai_config.h"
#include "net/ai_provider.h"

#include "ai_sidebar.xml.h"

#include <string>
#include <vector>

namespace ui {
  class TextBox;
  class View;
}

namespace app {

  class AiSidebar : public app::gen::AiSidebar {
  public:
    AiSidebar();
    ~AiSidebar();

    // Add a message to the chat history panel.
    // @param sender  Display name of the sender (e.g. "You", "AI")
    // @param text    Message body text
    void addMessage(const std::string& sender, const std::string& text);

    // Populate the model ComboBox from the current provider config.
    void refreshModelList();

    // Returns the currently selected model ID string.
    std::string selectedModelId() const;

    // Returns the current prompt text.
    std::string promptText() const;

    // Clear the prompt entry field.
    void clearPrompt();

  private:
    void onSendClick();
    void onUploadClick();
    void onModelChange();

    // Single TextBox widget inside the scrollable chat_view.
    // Using TextBox directly (instead of nested VBox containers)
    // so that View::getView(textbox) works and word-wrap functions correctly.
    ui::TextBox* m_chatContainer;

    // AI configuration (provider, model, api key)
    net::AiConfig m_config;

    // Cached model list for current provider
    std::vector<net::AiModelInfo> m_models;
  };

} // namespace app
