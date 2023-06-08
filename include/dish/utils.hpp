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

#include "bundled/tinyutf8/tinyutf8.h"

#include <list>
#include <vector>
#include <string>
#include <set>
#include <optional>
#include <filesystem>

#define FMT_HEADER_ONLY
#include "bundled/fmt/core.h"
#include "bundled/fmt/xchar.h"

template <> struct fmt::formatter<tiny_utf8::string> : formatter<std::string> {
  auto format(tiny_utf8::string c, format_context& ctx) {
    return formatter<std::string>::format(c.cpp_str(), ctx);
  }
};

namespace dish::utils
{
  enum class Effect : std::size_t
  {
    bold = 1, faint, italic, underline, slow_blink, rapid_blink, color_reverse,
    fg_black = 30, fg_red, fg_green, fg_yellow, fg_blue, fg_magenta, fg_cyan, fg_white,
    bg_black = 40, bg_red, bg_green, bg_yellow, bg_blue, bg_magenta, bg_cyan, bg_white,
    bg_shadow, bg_strong_shadow
  };
  
  enum class CommandType
  {
    not_found, builtin, lua_func, executable_file, executable_link,
    not_executable
  };
  struct Command
  {
    tiny_utf8::string name;
    CommandType type;
    size_t file_size;
  };

  tiny_utf8::string effect(const tiny_utf8::string &str, Effect effect);
  template<typename ...Args>
  tiny_utf8::string effect(const tiny_utf8::string &str, Effect e, Args&& ...effects)
  {
    if (str.empty()) return "";
    return effect(effect(str, e), effects...);
  }
  tiny_utf8::string red(const tiny_utf8::string &str);
  tiny_utf8::string green(const tiny_utf8::string &str);
  tiny_utf8::string yellow(const tiny_utf8::string &str);
  tiny_utf8::string blue(const tiny_utf8::string &str);
  tiny_utf8::string magenta(const tiny_utf8::string &str);
  tiny_utf8::string cyan(const tiny_utf8::string &str);
  tiny_utf8::string white(const tiny_utf8::string &str);
  
  tiny_utf8::string get_dish_env(const tiny_utf8::string &s);
  bool has_wildcards(const tiny_utf8::string &s);

  std::optional<tiny_utf8::string> get_home();
  std::optional<std::vector<tiny_utf8::string>> expand(const tiny_utf8::string &str);
  

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
  auto&& list_at(const std::list<T>& list, size_t index)
  {
    auto it = list.begin();
    std::advance(it, index);
    return *it;
  }

  tiny_utf8::string get_timestamp();

  tiny_utf8::string to_string(CommandType ct);

  bool operator<(const Command& a, const Command& b);

  bool is_executable(const std::filesystem::path& path);

  bool begin_with(const tiny_utf8::string &a, const tiny_utf8::string &b);

  std::set<Command> match_command(const tiny_utf8::string& pattern);

  std::tuple<CommandType, tiny_utf8::string> find_command(const tiny_utf8::string& cmd);

  tiny_utf8::string get_human_readable_size(size_t sz);

  tiny_utf8::string tilde(const tiny_utf8::string &path);

  std::vector<tiny_utf8::string> match_files_and_dirs(const tiny_utf8::string& path);


  size_t display_width(const tiny_utf8::string::const_iterator& beg, const tiny_utf8::string::const_iterator& end);

  size_t display_width(const tiny_utf8::string &str);

  template<typename ...Args>
  size_t display_width(const tiny_utf8::string &str, Args&& ...args)
  {
    return display_width(str) + display_width(std::forward<Args>(args)...);
  }

  tiny_utf8::string shrink_path(const tiny_utf8::string &path);
}
#endif