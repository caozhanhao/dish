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
#ifndef DISH_UTILS_HPP
#define DISH_UTILS_HPP
#pragma once

#include <list>
#include <vector>
#include <string>
#include <set>
#include <optional>
#include <filesystem>

#define FMT_HEADER_ONLY
#include "bundled/fmt/core.h"


namespace dish::utils
{
  enum class Effect : std::size_t
  {
    bold = 1, faint, italic, underline, slow_blink, rapid_blink, color_reverse,
    fg_black = 30, fg_red, fg_green, fg_yellow, fg_blue, fg_magenta, fg_cyan, fg_white,
    bg_black = 40, bg_red, bg_green, bg_yellow, bg_blue, bg_magenta, bg_cyan, bg_white
  };
  
  enum class CommandType
  {
    not_found, builtin, lua_func, executable_file, executable_link,
    not_executable
  };
  struct Command
  {
    std::string name;
    CommandType type;
    size_t file_size;
  };

  std::string effect(const std::string &str, Effect effect);
  template<typename ...Args>
  std::string effect(const std::string &str, Effect e, Args&& ...effects)
  {
    if (str.empty()) return "";
    return effect(effect(str, e), effects...);
  }
  std::string red(const std::string &str);
  std::string green(const std::string &str);
  std::string yellow(const std::string &str);
  std::string blue(const std::string &str);
  std::string magenta(const std::string &str);
  std::string cyan(const std::string &str);
  std::string white(const std::string &str);
  
  std::string get_dish_env(const std::string &s);
  bool has_wildcards(const std::string &s);

  std::optional<std::string> get_home();
  
  std::optional<std::vector<std::string>> expand(const std::string &str);
  
  std::string simplify_path(const std::string &path);

  template<typename T>
  T split(std::string_view str, std::string_view delims = " ")
  {
    T ret;
    size_t first = 0;
    while (first < str.size())
    {
      const auto second = str.find_first_of(delims, first);
      if (first != second)
        ret.insert(ret.end(), typename T::value_type{str.substr(first, second - first)});
      if (second == std::string_view::npos)
        break;
      first = second + 1;
    }
    return ret;
  }

  template<typename T>
  auto&& list_at(const std::list<T>& list, size_t index)
  {
    auto it = list.begin();
    std::advance(it, index);
    return *it;
  }

  std::string get_timestamp();
  
  std::string to_string(CommandType ct);

  bool operator<(const Command& a, const Command& b);

  bool is_executable(const std::filesystem::path& path);

  bool begin_with(const std::string& a, const std::string& b);

  std::set<Command> match_command(const std::string& pattern);

  std::tuple<CommandType, std::string> find_command(const std::string& cmd);

  std::string get_human_readable_size(size_t sz);

  std::vector<std::string> match_files_and_dirs(const std::string& path);

  size_t get_length_without_ansi_escape(const std::string& str);
}
#endif