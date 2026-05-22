// MonoSprite AI Extension
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/ai_sidebar.h"

#include "app/ui/status_bar.h"
#include "base/bind.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/combobox.h"
#include "ui/entry.h"
#include "ui/label.h"
#include "ui/listitem.h"
#include "ui/message.h"
#include "ui/system.h"
#include "ui/textbox.h"
#include "ui/view.h"

namespace app {

using namespace ui;

AiSidebar::AiSidebar()
  : m_chatContainer(nullptr)
{
  // Load AI configuration from .ini file
  m_config.load();

  // Use a single TextBox with WORDWRAP as the chat display.
  // Attaching it directly to the View ensures View::getView(textbox)
  // returns the correct View, which is required for word-wrap to work
  // (TextBox::onSizeHint uses the viewport width for line-breaking).
  // Using LEFT alignment ensures text starts at top-left.
  m_chatContainer = new TextBox("", LEFT | WORDWRAP);

  chatView()->attachToView(m_chatContainer);

  // Populate the model ComboBox with available models
  refreshModelList();

  // Connect button click signals
  sendBtn()->Click.connect(base::Bind<void>(&AiSidebar::onSendClick, this));
  uploadBtn()->Click.connect(base::Bind<void>(&AiSidebar::onUploadClick, this));
  modelSelector()->Change.connect(base::Bind<void>(&AiSidebar::onModelChange, this));

  // Show a welcome message
  addMessage("System", "AI Assistant ready. Select a model and type your prompt.");
}

AiSidebar::~AiSidebar()
{
  // Save current AI config when sidebar is destroyed
  m_config.save();
}

void AiSidebar::addMessage(const std::string& sender, const std::string& text)
{
  // Build the formatted message string
  std::string formatted;
  if (!m_chatContainer->text().empty()) {
    formatted = m_chatContainer->text() + "\n\n";
  }
  formatted += ">> " + sender + ":\n" + text;

  // Update the single TextBox content
  m_chatContainer->setText(formatted);

  // Scroll to the bottom so the latest message is visible
  chatView()->updateView();

  gfx::Size scrollable = chatView()->getScrollableSize();
  gfx::Size visible = chatView()->visibleSize();
  int maxScroll = scrollable.h - visible.h;
  if (maxScroll > 0) {
    chatView()->setViewScroll(gfx::Point(0, maxScroll));
  }
}

void AiSidebar::refreshModelList()
{
  modelSelector()->removeAllItems();

  // Get models for the current provider
  m_models = net::getDefaultModels(m_config.providerType());

  if (m_models.empty()) {
    // If no built-in models, show a placeholder
    modelSelector()->addItem("(No models available)");
    return;
  }

  int selectedIdx = 0;
  for (size_t i = 0; i < m_models.size(); ++i) {
    modelSelector()->addItem(m_models[i].displayName);
    if (m_models[i].id == m_config.selectedModelId()) {
      selectedIdx = static_cast<int>(i);
    }
  }
  modelSelector()->setSelectedItemIndex(selectedIdx);
}

std::string AiSidebar::selectedModelId() const
{
  int idx = modelSelector()->getSelectedItemIndex();
  if (idx >= 0 && idx < static_cast<int>(m_models.size())) {
    return m_models[idx].id;
  }
  return "";
}

std::string AiSidebar::promptText() const
{
  return promptEntry()->text();
}

void AiSidebar::clearPrompt()
{
  promptEntry()->setText("");
}

void AiSidebar::onSendClick()
{
  std::string prompt = promptText();
  if (prompt.empty()) {
    StatusBar::instance()->showTip(2000, "Please enter a prompt first");
    return;
  }

  // Show the user's message in the chat panel
  addMessage("You", prompt);
  clearPrompt();

  // Placeholder: In Task 4 this will call the AI API.
  // For now, show a placeholder response to verify the UI works.
  addMessage("AI", "[Task 4 will connect this to the AI backend]\nModel: "
             + selectedModelId());
}

void AiSidebar::onUploadClick()
{
  // Placeholder: In Task 5 this will open a file dialog.
  addMessage("System", "[Upload feature will be implemented in Task 5]");
}

void AiSidebar::onModelChange()
{
  std::string modelId = selectedModelId();
  if (!modelId.empty()) {
    m_config.setSelectedModelId(modelId);
    m_config.save();
  }
}

} // namespace app

