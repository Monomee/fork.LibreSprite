// MonoSprite AI Extension
// Copyright (C) 2026 MonoSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/cmd/set_palette.h"
#include "app/commands/command.h"
#include "app/console.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/file/file.h"
#include "app/modules/palettes.h"
#include "app/transaction.h"
#include "app/ui/status_bar.h"
#include "app/ui_context.h"
#include "base/base64.h"
#include "base/fs.h"
#include "base/path.h"
#include "doc/palette.h"
#include "net/ai_client.h"
#include "net/ai_config.h"
#include "render/quantization.h"
#include "ui/manager.h"
#include "ui/timer.h"

#include "ai_palette_generator.xml.h"

#include <memory>
#include <fstream>
#include <mutex>
#include <vector>

namespace app {

class PaletteGeneratorTask {
public:
  PaletteGeneratorTask(Context* context, const std::string& prompt, int colorsCount)
    : m_context(context)
    , m_prompt(prompt)
    , m_colorsCount(colorsCount)
    , m_isFinished(false)
  {
    m_timer.reset(new ui::Timer(100));
    m_timer->Tick.connect(&PaletteGeneratorTask::onTick, this);
  }

  void start() {
    m_config.load();
    m_client.setConfig(&m_config);

    StatusBar::instance()->showTip(10000, "Generating AI Palette... Please wait");

    // Start checking on main thread
    m_timer->start();

    // Call async API
    m_client.textToImage(
      m_prompt, 128, 128,
      [this](const net::AiResponse& response) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_response = response;
        m_isFinished = true;
      }
    );
  }

private:
  void onTick() {
    bool finished = false;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      finished = m_isFinished;
    }

    if (finished) {
      m_timer->stop();
      processResult();
      delete this; // Auto-delete task when done
    }
  }

  void processResult() {
    net::AiResponse resp;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      resp = m_response;
    }

    if (resp.status != net::AiStatus::Success) {
      std::string err = "AI Palette generation failed: ";
      if (!resp.errorMessage.empty()) {
        err += resp.errorMessage;
      } else {
        err += "Unknown error (status code " + std::to_string(resp.httpStatusCode) + ")";
      }
      StatusBar::instance()->showTip(5000, err.c_str());
      return;
    }

    if (resp.imageBase64.empty()) {
      StatusBar::instance()->showTip(5000, "AI Palette generation failed: No image returned");
      return;
    }

    try {
      // 1. Decode base64 to png bytes
      base::buffer pngBytes;
      base::decode_base64(resp.imageBase64, pngBytes);

      // 2. Write to temp file
      std::string tempDir = base::get_temp_path();
      std::string tempFile = base::join_path(tempDir, "ai_palette_temp.png");

      std::ofstream f(tempFile, std::ios::binary);
      if (!f.is_open()) {
        StatusBar::instance()->showTip(5000, "Failed to create temporary file for palette extraction");
        return;
      }
      f.write(reinterpret_cast<const char*>(pngBytes.data()), pngBytes.size());
      f.close();

      // 3. Load temp image as document
      std::unique_ptr<Document> tempDoc(app::load_document(nullptr, tempFile.c_str()));
      if (!tempDoc || !tempDoc->sprite()) {
        StatusBar::instance()->showTip(5000, "Failed to load generated temporary image");
        base::delete_file(tempFile);
        return;
      }

      // 4. Get image from temp doc and extract colors
      std::vector<Image*> images;
      tempDoc->sprite()->getImages(images);
      if (images.empty()) {
        StatusBar::instance()->showTip(5000, "No image data found in generated temporary file");
        base::delete_file(tempFile);
        return;
      }

      Image* tempImage = images.front();
      render::PaletteOptimizer optimizer;
      optimizer.feedWithImage(tempImage, true);

      auto newPalette = doc::Palette::create(m_colorsCount);
      optimizer.calculate(*newPalette, -1, nullptr);

      // 5. Apply palette via Transaction on current Sprite
      {
        ContextWriter writer(m_context, 500);
        Document* document(writer.document());
        Sprite* sprite = writer.sprite();

        if (document && sprite) {
          frame_t frame = writer.frame();

          Transaction transaction(writer.context(), "AI Palette Generation", ModifyDocument);
          transaction.execute(new cmd::SetPalette(sprite, frame, *newPalette));
          transaction.commit();

          set_current_palette(newPalette.get(), false);
          ui::Manager::getDefault()->invalidate();

          StatusBar::instance()->showTip(3000, "AI Palette generated successfully!");
        } else {
          StatusBar::instance()->showTip(5000, "No active document found to apply palette");
        }
      }

      // 6. Cleanup
      base::delete_file(tempFile);

    } catch (std::exception& e) {
      Console::showException(e);
    }
  }

  Context* m_context;
  std::string m_prompt;
  int m_colorsCount;
  bool m_isFinished;
  net::AiConfig m_config;
  net::AiClient m_client;
  net::AiResponse m_response;
  std::mutex m_mutex;
  std::unique_ptr<ui::Timer> m_timer;
};

class PaletteGeneratorCommand : public Command {
public:
  PaletteGeneratorCommand();

protected:
  bool onEnabled(Context* context) override;
  void onExecute(Context* context) override;
};

PaletteGeneratorCommand::PaletteGeneratorCommand()
  : Command("PaletteGenerator",
            "Smart Palette Generator",
            CmdUIOnlyFlag)
{
}

bool PaletteGeneratorCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable);
}

void PaletteGeneratorCommand::onExecute(Context* context)
{
  try {
    app::gen::AiPaletteGenerator window;
    window.openWindowInForeground();

    if (window.closer() == window.generate()) {
      std::string prompt = window.prompt()->text();
      int colorsCount = window.colorsCount()->textInt();

      if (colorsCount < 1) colorsCount = 1;
      if (colorsCount > 256) colorsCount = 256;

      if (prompt.empty()) {
        StatusBar::instance()->showTip(3000, "Prompt cannot be empty");
        return;
      }

      // Start async generation task
      auto task = new PaletteGeneratorTask(context, prompt, colorsCount);
      task->start();
    }
  }
  catch (base::Exception& e) {
    Console::showException(e);
  }
}

Command* CommandFactory::createPaletteGeneratorCommand()
{
  return new PaletteGeneratorCommand;
}

} // namespace app
