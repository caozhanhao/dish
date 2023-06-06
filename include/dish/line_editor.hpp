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
    std::string cmd;
    std::string timestamp;
  };

  struct LineEditorContext
  {
    std::vector<History> history;
    std::string line;
    std::string hint;
    size_t pos;
    size_t history_pos;

    size_t last_refresh_pos;

    std::string searching_history_pattern;
  };

  extern LineEditorContext dle_context;

  void dle_init();

  std::string read_line(const std::string &prompt);

  int save_history(const std::string &path);

  int load_history(const std::string &path);

}
#endif