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

namespace wiesel::editor {

TextEditor::LanguageDefinition CreateCSharpLanguageDefinition() {
  TextEditor::LanguageDefinition langDef;

  // C# keywords
  static const char* const keywords[] = {
      "abstract",
      "as",
      "base",
      "bool",
      "break",
      "byte",
      "case",
      "catch",
      "char",
      "checked",
      "class",
      "const",
      "continue",
      "decimal",
      "default",
      "delegate",
      "do",
      "double",
      "else",
      "enum",
      "event",
      "explicit",
      "extern",
      "false",
      "finally",
      "fixed",
      "float",
      "for",
      "foreach",
      "goto",
      "if",
      "implicit",
      "in",
      "int",
      "interface",
      "internal",
      "is",
      "lock",
      "long",
      "namespace",
      "new",
      "null",
      "object",
      "operator",
      "out",
      "override",
      "params",
      "private",
      "protected",
      "public",
      "readonly",
      "ref",
      "return",
      "sbyte",
      "sealed",
      "short",
      "sizeof",
      "stackalloc",
      "static",
      "string",
      "struct",
      "switch",
      "this",
      "throw",
      "true",
      "try",
      "typeof",
      "uint",
      "ulong",
      "unchecked",
      "unsafe",
      "ushort",
      "using",
      "var",
      "virtual",
      "void",
      "volatile",
      "while",
      // Contextual keywords
      "async",
      "await",
      "dynamic",
      "get",
      "global",
      "nameof",
      "partial",
      "set",
      "value",
      "when",
      "where",
      "yield",
  };
  for (auto& k : keywords) {
    langDef.mKeywords.insert(k);
  }

  // Only built-in .NET types - engine types come from the LSP
  static const char* const identifiers[] = {
      "String",    "Math",   "Console", "List", "Dictionary", "Array",
      "Exception", "Action", "Func",    "Task", "Object",     "Type",
  };
  for (const char* k : identifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "";
    langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  // C# uses the same tokenizer as C/C++ (C-style strings, numbers, identifiers)
  langDef.mTokenize = nullptr;  // uses default regex-based tokenizer

  // Token regex patterns for things the default tokenizer handles
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\$\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "@\\\"[^\\\"]*\\\"", TextEditor::PaletteIndex::String));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fFdDmM]?",
          TextEditor::PaletteIndex::Number));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?0[xX][0-9a-fA-F]+[uUlL]*", TextEditor::PaletteIndex::Number));
  langDef.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
  langDef.mTokenRegexStrings.push_back(std::make_pair<std::string,
                                                      TextEditor::PaletteIndex>(
      "[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]",
      TextEditor::PaletteIndex::Punctuation));

  langDef.mCommentStart = "/*";
  langDef.mCommentEnd = "*/";
  langDef.mSingleLineComment = "//";

  langDef.mCaseSensitive = true;
  langDef.mAutoIndentation = true;
  langDef.mPreprocChar = '#';

  langDef.mName = "C#";

  return langDef;
}

}  // namespace wiesel::editor