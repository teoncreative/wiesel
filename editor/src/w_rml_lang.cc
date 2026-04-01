//
//   Copyright 2026 Metehan Gezer
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//

#include "TextEditor.h"

namespace Wiesel::Editor {

TextEditor::LanguageDefinition CreateRmlLanguageDefinition() {
  TextEditor::LanguageDefinition langDef;

  // HTML/RML tag names and attributes as keywords
  static const char* const keywords[] = {
      "rml",
      "head",
      "body",
      "title",
      "link",
      "style",
      "div",
      "span",
      "p",
      "h1",
      "h2",
      "h3",
      "h4",
      "img",
      "input",
      "button",
      "select",
      "option",
      "textarea",
      "form",
      "table",
      "tr",
      "td",
      "th",
      "br",
      "hr",
      "progress",
      "tab",
      "tabs",
      "panel",
      "tabset",
      "handle",
      "scrollbarvertical",
      "scrollbarhorizontal",
  };
  for (const char* k : keywords) {
    langDef.mKeywords.insert(k);
  }

  // HTML/RML attributes as identifiers
  static const char* const identifiers[] = {
      "class",
      "id",
      "style",
      "type",
      "href",
      "src",
      "value",
      "max",
      "min",
      "name",
      "data-model",
      "data-if",
      "data-for",
      "data-value",
      "data-event-click",
      "data-event-change",
      "data-style-width",
      "data-style-height",
      "data-style-color",
      "data-attr-value",
      "data-attr-max",
      "data-attr-src",
      "data-class-active",
      "direction",
  };
  for (const char* k : identifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "";
    langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  // Regex patterns for tokenizing
  // Strings (attribute values)
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'(\\\\.|[^\\'])*\\'", TextEditor::PaletteIndex::String));
  // Data binding expressions {{...}}
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\{\\{[^\\}]*\\}\\}", TextEditor::PaletteIndex::Preprocessor));
  // Numbers
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)",
          TextEditor::PaletteIndex::Number));
  // Tags: <tagname and </tagname
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "</?[a-zA-Z][a-zA-Z0-9-]*", TextEditor::PaletteIndex::Keyword));
  // Identifiers (attribute names)
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_-]*", TextEditor::PaletteIndex::Identifier));
  // Punctuation (angle brackets, equals, slash, etc.)
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[<>/=\\{\\}]", TextEditor::PaletteIndex::Punctuation));

  langDef.mCommentStart = "<!--";
  langDef.mCommentEnd = "-->";
  langDef.mSingleLineComment = "";

  langDef.mCaseSensitive = false;
  langDef.mAutoIndentation = true;

  langDef.mName = "RML";

  return langDef;
}

TextEditor::LanguageDefinition CreateRcssLanguageDefinition() {
  TextEditor::LanguageDefinition langDef;

  // CSS property names as keywords
  static const char* const keywords[] = {
      "display",
      "position",
      "top",
      "bottom",
      "left",
      "right",
      "width",
      "height",
      "min-width",
      "min-height",
      "max-width",
      "max-height",
      "margin",
      "margin-top",
      "margin-bottom",
      "margin-left",
      "margin-right",
      "padding",
      "padding-top",
      "padding-bottom",
      "padding-left",
      "padding-right",
      "border",
      "border-width",
      "border-color",
      "border-radius",
      "background",
      "background-color",
      "color",
      "font-family",
      "font-size",
      "font-weight",
      "font-style",
      "text-align",
      "text-decoration",
      "text-transform",
      "line-height",
      "overflow",
      "visibility",
      "opacity",
      "cursor",
      "float",
      "clear",
      "vertical-align",
      "white-space",
      "flex",
      "flex-direction",
      "flex-wrap",
      "justify-content",
      "align-items",
      "align-content",
      "gap",
      "row-gap",
      "column-gap",
      "order",
      "flex-grow",
      "flex-shrink",
      "box-sizing",
      "z-index",
      "decorator",
      "font-effect",
      "image-color",
      "tab-index",
      "direction",
      "drag",
      "focus",
  };
  for (const char* k : keywords) {
    langDef.mKeywords.insert(k);
  }

  // CSS values as identifiers
  static const char* const identifiers[] = {
      "block",    "inline",  "flex",     "none",        "absolute",
      "relative", "fixed",   "auto",     "hidden",      "visible",
      "scroll",   "center",  "left",     "right",       "top",
      "bottom",   "bold",    "italic",   "normal",      "inherit",
      "solid",    "dotted",  "dashed",   "transparent", "pointer",
      "row",      "column",  "wrap",     "nowrap",      "start",
      "end",      "stretch", "baseline", "border-box",  "content-box",
  };
  for (const char* k : identifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "";
    langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  // Regex patterns
  // Strings
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'(\\\\.|[^\\'])*\\'", TextEditor::PaletteIndex::String));
  // Hex colors (#fff, #ffffff, #ffffffff)
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "#[0-9a-fA-F]{3,8}", TextEditor::PaletteIndex::Number));
  // Numbers with units (dp, px, em, %, etc.)
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)(dp|px|em|rem|%)?",
          TextEditor::PaletteIndex::Number));
  // CSS functions: rgba(), rgb(), etc.
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z-]+\\(", TextEditor::PaletteIndex::Preprocessor));
  // Selectors and property names
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_-]*", TextEditor::PaletteIndex::Identifier));
  // Class/ID selectors
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[.#][a-zA-Z_][a-zA-Z0-9_-]*", TextEditor::PaletteIndex::Keyword));
  // Punctuation
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[\\{\\}:;,>+~]", TextEditor::PaletteIndex::Punctuation));

  langDef.mCommentStart = "/*";
  langDef.mCommentEnd = "*/";
  langDef.mSingleLineComment = "";

  langDef.mCaseSensitive = false;
  langDef.mAutoIndentation = true;

  langDef.mName = "RCSS";

  return langDef;
}

}  // namespace Wiesel::Editor
