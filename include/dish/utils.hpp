//   Copyright 2022 - 2025 dish - caozhanhao
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

#include <filesystem>
#include <list>
#include <optional>
#include <set>
#include <string>
#include <vector>

#define FMT_HEADER_ONLY
#include "bundled/fmt/core.h"
#include "bundled/fmt/xchar.h"

#include "type_alias.hpp"

template<>
struct fmt::formatter<dish::String> : formatter<std::string>
{
  auto format(dish::String c, format_context &ctx)
  {
    return formatter<std::string>::format(c.cpp_str(), ctx);
  }
};

namespace dish::utils
{
  enum class Effect : std::size_t
  {
    bold = 1,
    faint,
    italic,
    underline,
    slow_blink,
    rapid_blink,
    color_reverse,
    fg_black = 30,
    fg_red,
    fg_green,
    fg_yellow,
    fg_blue,
    fg_magenta,
    fg_cyan,
    fg_white,
    bg_black = 40,
    bg_red,
    bg_green,
    bg_yellow,
    bg_blue,
    bg_magenta,
    bg_cyan,
    bg_white,
    bg_shadow,
    bg_strong_shadow
  };

  enum class CommandType
  {
    not_found,
    builtin,
    lua_func,
    executable_file,
    executable_link,
    not_executable
  };
  struct Command
  {
    String name;
    CommandType type;
    size_t file_size;
  };

  String effect(const String &str, Effect effect);
  template<typename... Args>
  String effect(const String &str, Effect e, Args &&...effects)
  {
    if (str.empty()) return "";
    return effect(effect(str, e), effects...);
  }
  String red(const String &str);
  String green(const String &str);
  String yellow(const String &str);
  String blue(const String &str);
  String magenta(const String &str);
  String cyan(const String &str);
  String white(const String &str);

  String get_dish_env(const String &s);
  bool has_wildcards(const String &s);

  std::optional<String> get_home();

  std::optional<std::vector<String>> expand_wildcards(const String &s);

  std::vector<String> expand(const String &str);

  template<typename STR_VIEW, typename T>
  T split(STR_VIEW str, STR_VIEW delims = " ")
  {
    T ret;
    size_t first = 0;
    while (first < str.size())
    {
      const auto second = str.find_first_of(delims, first);
      if (first != second)
        ret.insert(ret.end(), typename T::value_type{std::string(str.substr(first, second - first))});
      if (second == std::string_view::npos)
        break;
      first = second + 1;
    }
    return ret;
  }

  template<typename T>
  auto &&list_at(const std::list<T> &list, size_t index)
  {
    auto it = list.begin();
    std::advance(it, index);
    return *it;
  }

  String get_timestamp();

  String to_string(CommandType ct);

  bool operator<(const Command &a, const Command &b);

  bool is_executable(const std::filesystem::path &path);

  std::tuple<CommandType, String> find_command(const String &cmd);

  String tilde(const String &path);

  // Dish Line Editor

  String get_human_readable_size(size_t sz);

  bool begin_with(const String &a, const String &b);

  std::set<Command> match_command(const String &pattern);

  std::vector<String> match_files_and_dirs(const String &path);


  size_t display_width(const String::const_iterator &beg, const String::const_iterator &end);

  size_t display_width(const String &str);

  template<typename... Args>
  size_t display_width(const String &str, Args &&...args)
  {
    return display_width(str) + display_width(std::forward<Args>(args)...);
  }

  String shrink_path(const String &path);
}// namespace dish::utils
#endif