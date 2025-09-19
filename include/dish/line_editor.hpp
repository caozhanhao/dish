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

#include "dish/type_alias.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace dish::line_editor
{
  struct History
  {
    String cmd;
    String timestamp;
  };

  struct CompletionCandidate
  {
    String completion;
    String info;
    String selection;
  };

  struct CompletionDisplay
  {
    String completion;
    String selection;
  };

  struct LineEditorContext
  {
    // edit
    String line;
    size_t pos;
    size_t last_cols;

    // history
    std::vector<History> history;
    size_t history_pos;
    String searching_history_pattern;

    //hint
    String hint;

    // complete
    String prompt;
    size_t completion_show_line_pos;
    size_t completion_show_line_size;
    String complete_pattern;// for highlight
    std::vector<std::vector<CompletionDisplay>> completion;
    bool searching_completion;
    size_t completion_pos_line;
    size_t completion_pos_column;
  };

  extern LineEditorContext dle_context;

  void dle_init();

  String read_line(const String &prompt);

  int save_history(const String &path);

  int load_history(const String &path);

}// namespace dish::line_editor
#endif