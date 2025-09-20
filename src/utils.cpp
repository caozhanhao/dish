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

#include "dish/utils.hpp"
#include "dish/builtin.hpp"
#include "dish/dish.hpp"

#include "dish/bundled/widecharwidth/widechar_width.h"

#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <list>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dish::utils
{
  String effect(const String &str, Effect effect_)
  {
    if (str.empty()) return "";
    if (effect_ == utils::Effect::bg_shadow)
      return fmt::format("\033[48;5;7m{}\033[49m", str);
    else if (effect_ == utils::Effect::bg_strong_shadow)
      return fmt::format("\033[48;5;8m{}\033[49m", str);

    int effect = static_cast<int>(effect_);
    int end = 0;
    if (effect >= 1 && effect <= 7)
      end = 0;
    else if (effect >= 30 && effect <= 37)
      end = 39;
    else if (effect >= 40 && effect <= 47)
      end = 49;
    return fmt::format("\033[{}m{}\033[{}m", effect, str, end);
  }

  String red(const String &str)
  {
    return effect(str, Effect::fg_red);
  }
  String green(const String &str)
  {
    return effect(str, Effect::fg_green);
  }
  String yellow(const String &str)
  {
    return effect(str, Effect::fg_yellow);
  }
  String blue(const String &str)
  {
    return effect(str, Effect::fg_blue);
  }
  String magenta(const String &str)
  {
    return effect(str, Effect::fg_magenta);
  }
  String cyan(const String &str)
  {
    return effect(str, Effect::fg_cyan);
  }
  String white(const String &str)
  {
    return effect(str, Effect::fg_white);
  }

  String get_dish_env(const String &s)
  {
    if (auto it = dish_context.lua_state["dish"]["environment"][s.cpp_str()]; it.valid())
      return {it.get<std::string>()};
    return "";
  }
  bool has_wildcards(const String &s)
  {
    return (s.find('*') != String::npos ||
            s.find('?') != String::npos);
  }

  std::optional<String> get_home()
  {
    String home;
    const std::vector<String> env_to_find{"HOME", "USERPROFILE", "HOMEDRIVE", "HOMEPATH"};
    for (auto &r: env_to_find)
    {
      if (auto it = dish_context.lua_state["dish"]["environment"][r.cpp_str()]; it.valid())
      {
        home = it.get<std::string>();
        break;
      }
    }
    if (home.empty()) return std::nullopt;
    if (*home.rbegin() == '/') home.pop_back();
    return home;
  }

  String get_dir_name(const std::filesystem::directory_entry &dir_entry)
  {
    if (dir_entry.is_directory())
      return std::filesystem::path(dir_entry.path().string()).filename().string() + "/";
    return dir_entry.path().filename().string();
  }

  std::optional<std::vector<String>> expand_wildcards(const String &str)
  {
    String name_to_match;
    std::filesystem::path path_to_match;
    if (str.find('/') != String::npos)
    {
      size_t wildcards_pos = 0;
      auto a = str.find('*');
      auto b = str.find('?');
      if (a != String::npos)
        wildcards_pos = a;
      if (b != String::npos && (a == String::npos || b < a))
        wildcards_pos = b;

      size_t beg = wildcards_pos;
      size_t end = wildcards_pos;
      while (beg > 0 && str[beg] != '/') --beg;
      while (end < str.length() && str[end] != '/') ++end;
      if (beg == 0)
      {
        name_to_match = str.substr(beg, end);
        path_to_match = std::filesystem::current_path();
      }
      else
      {
        name_to_match = str.substr(beg + 1, end);
        path_to_match = str.substr(0, beg + 1).cpp_str();
      }
    }
    else
    {
      path_to_match = std::filesystem::current_path();
      name_to_match = str;
    }

    String pattern{'^'};
    for (auto it = name_to_match.cbegin(); it < name_to_match.cend(); ++it)
    {
      auto ch = *it;
      switch (ch)
      {
        case '*':
          pattern += ".*";
          break;
        case '?':
          pattern += ".?";
          break;
        case '.':
          pattern += "\\.";
          break;
        default:
          pattern += ch;
          break;
      }
    }
    pattern += '$';

    std::vector<String> ret;
    try
    {
      std::regex re(pattern.cpp_str());
      for (auto &dir_entry: std::filesystem::directory_iterator{path_to_match})
      {
        auto name = get_dir_name(dir_entry);
        if (name.back() == '/') name.pop_back();
        if (std::regex_match(name.cpp_str(), re))
        {
          if (name[0] == '.')
          {
            if (name_to_match[0] == '.')
              ret.emplace_back(String{path_to_match.string()} + name);
          }
          else
            ret.emplace_back(String{path_to_match.string()} + name);
        }
      }
    } catch (...)
    {
      return std::nullopt;
    }
    for (auto &r: ret)
    {
      if (has_wildcards(r))
      {
        auto e = expand_wildcards(r);
        if (e.has_value())
          ret.insert(ret.end(), std::make_move_iterator(e->begin()), std::make_move_iterator(e->end()));
      }
    }
    std::sort(ret.begin(), ret.end());
    return ret;
  }

  String tilde(const String &path_)
  {
    String path = std::filesystem::path(path_.cpp_str()).lexically_normal().string();
    auto home_opt = get_home();
    if (!home_opt.has_value()) return path;
    auto home = home_opt.value();
    auto [it1, it2] = std::mismatch(path.cbegin(), path.cend(), home.cbegin(), home.cend());
    if (it2 == home.cend())
      return "~" + path.substr(home.length());
    return path;
  }

  String expand_tilde(const String &str)
  {
    String ret;
    auto home = get_home();
    if (!home.has_value())
      return "";
    for (auto it = str.cbegin(); it < str.cend(); ++it)
    {
      if (*it == '~' &&
          ((it + 1 != str.cend() && it != str.cbegin() && *(it + 1) == '/' && *(it - 1) == '/') || (it + 1 == str.cend() && it != str.cbegin() && *(it - 1) == '/') || (it + 1 != str.cend() && it == str.cbegin() && *(it + 1) == '/') || (it + 1 == str.cend() && it == str.cbegin())))
        ret += home.value();
      else
        ret += *it;
    }
    return ret;
  }

  std::vector<String> expand(const String &str)
  {
    if (str.empty()) return {};
    String s = expand_tilde(str);
    if (s.empty()) s = str;

    // Wildcards
    if (utils::has_wildcards(s))
    {
      auto w = expand_wildcards(s);
      if (w.has_value() && !w.value().empty())
        return *w;
    }
    return {s};
  }

  String get_timestamp()
  {
    auto tp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count());
  }

  String to_string(CommandType ct)
  {
    switch (ct)
    {
      case CommandType::not_found:
        return "not found";
        break;
      case CommandType::builtin:
        return "builtin";
        break;
      case CommandType::lua_func:
        return "lua function";
        break;
      case CommandType::executable_file:
        return "executable";
        break;
      case CommandType::executable_link:
        return "executable link";
        break;
      case CommandType::not_executable:
        return "file";
        break;
    }
    return "";
  }

  bool operator<(const Command &a, const Command &b)
  {
    return a.name < b.name;
  }

  bool is_executable(const std::filesystem::path &path)
  {
    auto p = std::filesystem::status(path).permissions();
    return (((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ||
            ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ||
            ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none));
  }

  bool begin_with(const String &a, const String &b)
  {
    if (a.length() < b.length()) return false;
    for (size_t i = 0; i < b.length(); ++i)
    {
      if (a[i] != b[i])
        return false;
    }
    return true;
  }


  std::tuple<CommandType, String> find_command(const String &cmd)
  {
    if (builtin::builtins.find(cmd) != builtin::builtins.end())
      return {CommandType::builtin, cmd};

    if (dish_context.lua_state["dish"]["func"][cmd.cpp_str()].valid())
      return {CommandType::lua_func, cmd};

    try // catch exceptions such as permission denied
    {
      String abs_path;
      auto found = [&]() -> bool {
        if (cmd.starts_with('/')) {
          abs_path = cmd;
          return std::filesystem::exists(abs_path.cpp_str());
        }

        auto paths = get_path();
        for (const auto &path: paths)
        {
          abs_path = path + "/" + cmd;
          if (std::filesystem::exists(abs_path.cpp_str()))
            return true;
        }
        return false;
      }();

      if (!found)
        return {CommandType::not_found, ""};
      if (!is_executable(abs_path.cpp_str()))
        return {CommandType::not_executable, abs_path};
      if (std::filesystem::is_symlink(std::filesystem::path(abs_path.cpp_str())))
        return {CommandType::executable_link, abs_path};
      return {CommandType::executable_file, abs_path};
    } catch (...)
    {
      return {CommandType::not_found, ""};
    }
    return {};
  }

  String get_human_readable_size(size_t sz)
  {
    int i = 0;
    double mantissa = sz;
    for (; mantissa >= 1024.; mantissa /= 1024., ++i) {
    }
    mantissa = std::ceil(mantissa * 10.) / 10.;
    return fmt::format("{}{}", mantissa, "BKMGTPE"[i]);
  }

  std::set<Command> match_command(const String &pattern)
  {
    static std::unordered_map<String, std::set<Command>> cache;
    static std::chrono::steady_clock::time_point last_cache_time = std::chrono::steady_clock::now();
    if (auto d = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_cache_time);
        d.count() > 2)
      cache.clear();
    if (auto it = cache.find(pattern); it != cache.end()) return it->second;
    std::set<Command> ret;
    // builtin
    for (auto &r: builtin::builtins)
    {
      if (begin_with(r.first, pattern))
        ret.insert(Command{r.first, CommandType::builtin, 0});
    }
    // lua function
    for (auto &r: dish_context.lua_state["dish"]["func"].get<sol::table>())
    {
      auto fn = String(r.first.as<std::string>());
      if (begin_with(fn, pattern))
        ret.insert(Command{fn, CommandType::lua_func, 0});
    }
    // PATH
    try
    {
      for (auto &path: get_path())
      {
        for (auto &dir_entry: std::filesystem::directory_iterator{path.cpp_str()})
        {
          if (dir_entry.is_directory() || !is_executable(dir_entry.path())) continue;
          if (begin_with(String(dir_entry.path().filename().string()), pattern))
          {
            auto size = std::filesystem::file_size(dir_entry.path());
            CommandType type = CommandType::executable_file;
            if (std::filesystem::is_symlink(dir_entry.path()))
              type = CommandType::executable_link;
            ret.insert(Command{dir_entry.path().filename().string(), type, size});
          }
        }
      }
    } catch (...)
    {
      return ret;
    }
    cache.insert({pattern, ret});
    last_cache_time = std::chrono::steady_clock::now();
    return ret;
  }


  std::vector<String> match_files_and_dirs_no_wildcards(const String &raw_complete)
  {
    String complete = expand_tilde(raw_complete);
    std::vector<String> ret;
    try // such as permission denied
    {
      auto curr = std::filesystem::current_path();
      std::filesystem::directory_iterator dit;
      String pattern_to_match;
      if (complete.empty() || complete.find('/') == String::npos)
      {
        // filename
        dit = std::filesystem::directory_iterator{curr};
        pattern_to_match = complete;
      }
      else {
        std::filesystem::path path(complete.cpp_str());
        // maybe broken filename
        if (!std::filesystem::exists(path.parent_path()))
          return {};
        pattern_to_match = expand_tilde(path.filename().string());
        if (!path.filename().empty() && pattern_to_match.empty())
          pattern_to_match = path.filename().string();
        dit = std::filesystem::directory_iterator{path.parent_path()};
      }

      bool match_hidden = !pattern_to_match.empty() && pattern_to_match[0] == '.';

      for (auto &dir_entry: dit)
      {
        auto abs = std::filesystem::absolute(dir_entry.path());
        String name = get_dir_name(dir_entry);
        if (name[0] == '.' && !match_hidden)
          continue;

        if (begin_with(name, pattern_to_match))
          ret.emplace_back(name);
      }
    } catch (...)
    {
      return ret;
    }
    return ret;
  }

  std::vector<String> match_files_and_dirs(const String &complete)
  {
    static std::unordered_map<String, std::vector<String>> cache;
    static std::chrono::steady_clock::time_point last_cache_time = std::chrono::steady_clock::now();
    if (auto d = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_cache_time);
        d.count() > 2)
      cache.clear();
    auto curr = std::filesystem::current_path();
    String curr_path_str = curr.string();
    auto cache_key = curr_path_str + "  |  " + complete;
    if (auto it = cache.find(cache_key); it != cache.end())
      return it->second;

    std::vector<String> ret;

    if (has_wildcards(complete))
    {
      auto expanded = expand(complete);
      String pattern = expand_tilde(complete);
      for (auto &r: expanded)
      {
        auto [it1, it2] = std::mismatch(r.cbegin(), r.cend(), pattern.cbegin(), pattern.cend());
        ret.emplace_back(r.substr(it1.get_index()));
      }
    }
    else
      ret = match_files_and_dirs_no_wildcards(complete);
    cache.insert({cache_key, ret});
    last_cache_time = std::chrono::steady_clock::now();
    return ret;
  }


  size_t display_width(const String::const_iterator &beg, const String::const_iterator &end)
  {
    // TODO Optimization
    String no_ansi;
    for (auto it = beg; it < end; ++it)
    {
      if (*it == '\033')
      {
        while (it < end && *it != 'm') ++it;
        continue;
      }
      no_ansi += *it;
    }
    std::u32string wide(no_ansi.length(), 0);
    no_ansi.to_wide_literal(&wide[0]);
    size_t width = 0;
    for (auto &r: wide)
      width += widechar_wcwidth(r);
    return width;
  }


  size_t display_width(const String &str)
  {
    return display_width(str.cbegin(), str.cend());
  }

  String shrink_path(const String &path)
  {
    if (path == "/") return "/";
    String ret;
    if (path[0] != '/')
      ret += path[0];
    ret += "/";

    for (auto it = path.cbegin(); it < path.cend(); ++it)
    {
      if (it - 1 != path.cbegin() && *(it - 1) == '/')
      {
        ret += *it;
        ret += "/";
      }
    }
    ret.pop_back();
    if (auto i = path.rfind('/'); i != String::npos && i != path.length() - 1)
      ret.insert(ret.end(), path.substr(i + 2));
    return ret;
  }
}// namespace dish::utils