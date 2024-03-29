//   Copyright 2022 - 2023 dish - caozhanhao
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.

// The Dish Line Editor

#ifndef DISH_LINE_EDITOR_HPP
#define DISH_LINE_EDITOR_HPP
#pragma once

#include <string>
#include <chrono>
#include <vector>

namespace dish::line_editor
{
  struct History
  {
    tiny_utf8::string cmd;
    tiny_utf8::string timestamp;
  };

  struct CompletionCandidate
  {
    tiny_utf8::string completion;
    tiny_utf8::string info;
    tiny_utf8::string selection;
  };

  struct CompletionDisplay
  {
    tiny_utf8::string completion;
    tiny_utf8::string selection;
  };

  struct LineEditorContext
  {
    // edit
    tiny_utf8::string line;
    size_t pos;
    size_t last_cols;

    // history
    std::vector<History> history;
    size_t history_pos;
    tiny_utf8::string searching_history_pattern;

    //hint
    tiny_utf8::string hint;

    // complete
    tiny_utf8::string prompt;
    size_t completion_show_line_pos;
    size_t completion_show_line_size;
    tiny_utf8::string complete_pattern; // for highlight
    std::vector<std::vector<CompletionDisplay>> completion;
    bool searching_completion;
    size_t completion_pos_line;
    size_t completion_pos_column;
  };

  extern LineEditorContext dle_context;

  void dle_init();

  tiny_utf8::string read_line(const tiny_utf8::string &prompt);

  int save_history(const tiny_utf8::string &path);

  int load_history(const tiny_utf8::string &path);

}
#endif