//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#include "TextEditor.h"
#include "w_lsp_client.h"

namespace Wiesel::Editor {

class LspAutocompleteProvider : public TextEditor::IAutocompleteProvider {
 public:
  explicit LspAutocompleteProvider(LspClient& client) : client_(client) {}

  void RequestCompletions(const std::string& filePath, int line, int column,
                          const std::string& prefix) override {
    std::string uri = LspClient::PathToUri(filePath);
    client_.RequestCompletion(uri, line, column);
  }

  void OnTextChanged(const std::string& filePath,
                     const std::string& fullText) override {
    std::string uri = LspClient::PathToUri(filePath);
    client_.DidChange(uri, fullText);
  }

  bool HasResults() override { return client_.HasCompletions(); }

  std::vector<TextEditor::CompletionItem> TakeResults() override {
    auto lsp_items = client_.TakeCompletions();
    std::vector<TextEditor::CompletionItem> items;
    items.reserve(lsp_items.size());
    for (auto& li : lsp_items) {
      items.push_back({li.label, li.detail, li.insert_text,
                       static_cast<TextEditor::CompletionKind>(li.kind)});
    }
    return items;
  }

 private:
  LspClient& client_;
};

}  // namespace Wiesel::Editor