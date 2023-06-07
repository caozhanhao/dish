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

#include "dish/utils.hpp"
#include "dish/dish.hpp"
#include "dish/builtin.hpp"

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>
#include <list>
#include <set>
#include <algorithm>
#include <string>
#include <string_view>
#include <regex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <codecvt>

namespace dish::utils
{
  tiny_utf8::string effect(const tiny_utf8::string &str, Effect effect_)
  {
    if (str.empty()) return "";
    if(effect_ == utils::Effect::bg_shadow)
      return fmt::format("\033[48;5;7m{}\033[49m", str);
    else if(effect_ == utils::Effect::bg_strong_shadow)
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

  tiny_utf8::string red(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_red);
  }
  tiny_utf8::string green(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_green);
  }
  tiny_utf8::string yellow(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_yellow);
  }
  tiny_utf8::string blue(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_blue);
  }
  tiny_utf8::string magenta(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_magenta);
  }
  tiny_utf8::string cyan(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_cyan);
  }
  tiny_utf8::string white(const tiny_utf8::string &str)
  {
    return effect(str, Effect::fg_white);
  }

  tiny_utf8::string get_dish_env(const tiny_utf8::string &s)
  {
    if (auto it = dish_context.lua_state["dish"]["environment"][s.cpp_str()]; it.valid())
      return {it.get<std::string>()};
    return "";
  }
  bool has_wildcards(const tiny_utf8::string &s)
  {
    return (s.find('*') != tiny_utf8::string::npos ||
            s.find('?') != tiny_utf8::string::npos);
  }

  std::optional<tiny_utf8::string> get_home()
  {
    tiny_utf8::string home;
    const std::vector<tiny_utf8::string> env_to_find{"HOME", "USERPROFILE", "HOMEDRIVE", "HOMEPATH"};
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

  std::optional<std::vector<tiny_utf8::string>> expand(const tiny_utf8::string &str)
  {
    tiny_utf8::string s;
    auto home = get_home();
    // Tilde(~)
    if (home.has_value())
    {
      for (auto it = str.cbegin(); it < str.cend(); ++it)
      {
        if (*it == '~' &&
            ((it + 1 != str.cend() && it != str.cbegin() && *(it + 1) == '/' && *(it - 1) == '/') || (it + 1 == str.cend() && it != str.cbegin() && *(it - 1) == '/') || (it + 1 != str.cend() && it == str.cbegin() && *(it + 1) == '/') || (it + 1 == str.cend() && it == str.cbegin())))
          s += home.value();
        else
          s += *it;
      }
    }
    else
      s = str;
    // Wildcards
    if (utils::has_wildcards(str))
    {
      tiny_utf8::string pattern{'^'};
      for (auto it = s.cbegin(); it < s.cend(); ++it)
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

      DIR *dir = opendir(".");
      if (dir == NULL)
      {
        fmt::println("opendir: {}", strerror(errno));
        return std::nullopt;
      }

      std::regex re(pattern.cpp_str());
      std::vector<tiny_utf8::string> ret;
      dirent *ent;
      while ((ent = readdir(dir)) != NULL)
      {
        if (std::regex_match(ent->d_name, re))
        {
          if (ent->d_name[0] == '.')
          {
            if (s[0] == '.')
            {
              ret.emplace_back(ent->d_name);
            }
          }
          else
          {
            ret.emplace_back(ent->d_name);
          }
        }
      }
      closedir(dir);
      std::sort(ret.begin(), ret.end());
      return ret;
    }
    return {{s}};
  }

  tiny_utf8::string get_timestamp()
  {
    auto tp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count());
  }

  tiny_utf8::string to_string(CommandType ct)
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

  std::set<Command> match_command(const tiny_utf8::string &pattern)
  {
    std::set<Command> ret;
    // PATH
    for (auto &path: get_path())
    {
      for (auto &dir_entry: std::filesystem::directory_iterator{path.cpp_str()})
      {
        if (dir_entry.is_directory() || !is_executable(dir_entry.path())) continue;
        if (begin_with(tiny_utf8::string(dir_entry.path().filename().string()), pattern))
        {
          auto size = std::filesystem::file_size(dir_entry.path());
          CommandType type = CommandType::executable_file;
          if (std::filesystem::is_symlink(dir_entry.path()))
            type = CommandType::executable_link;
          ret.insert(Command{dir_entry.path().filename().string(), type, size});
        }
      }
    }
    // builtin
    for (auto &r: builtin::builtins)
    {
      if (begin_with(r.first, pattern))
        ret.insert(Command{r.first, CommandType::builtin, 0});
    }
    // lua function
    for (auto &r: dish_context.lua_state["dish"]["func"].get<sol::table>())
    {
      auto fn = tiny_utf8::string(r.first.as<std::string>());
      if (begin_with(fn, pattern))
        ret.insert(Command{fn, CommandType::lua_func, 0});
    }
    return ret;
  }

  std::tuple<CommandType, tiny_utf8::string> find_command(const tiny_utf8::string &cmd)
  {
    if (builtin::builtins.find(cmd) != builtin::builtins.end())
      return {CommandType::builtin, cmd};

    if (dish_context.lua_state["dish"]["func"][cmd.cpp_str()].valid())
      return {CommandType::lua_func, cmd};

    bool found = false;
    tiny_utf8::string abs_path;
    for (auto path: get_path())
    {
      abs_path = path + "/" + cmd;
      if (std::filesystem::exists(abs_path.cpp_str()))
      {
        found = true;
        break;
      }
    }
    if (!found)
      return {CommandType::not_found, ""};
    if (!is_executable(abs_path.cpp_str()))
      return {CommandType::not_executable, abs_path};
    if (std::filesystem::is_symlink(std::filesystem::path(abs_path.cpp_str())))
      return {CommandType::executable_link, abs_path};
    return {CommandType::executable_file, abs_path};
  }

  tiny_utf8::string get_human_readable_size(size_t sz)
  {
    int i = 0;
    double mantissa = sz;
    for (; mantissa >= 1024.; mantissa /= 1024., ++i) {
    }
    mantissa = std::ceil(mantissa * 10.) / 10.;
    return fmt::format("{}{}", mantissa, "BKMGTPE"[i]);
  }

  tiny_utf8::string tilde(const tiny_utf8::string &path_)
  {
    tiny_utf8::string path = std::filesystem::path(path_.cpp_str()).lexically_normal().string();
    auto home_opt = get_home();
    if (!home_opt.has_value()) return path;
    auto home = home_opt.value();
    auto [it1, it2] = std::mismatch(path.cbegin(), path.cend(), home.cbegin(), home.cend());
    if (it2 == home.cend())
      return "~" + path.substr(home.size());
    return path;

  }

  tiny_utf8::string tilde_expand(const tiny_utf8::string& str)
  {
    tiny_utf8::string ret;
    auto home = get_home().value();
    for (auto it = str.cbegin(); it < str.cend(); ++it)
    {
      if (*it == '~' &&
          ((it + 1 != str.cend() && it != str.cbegin() && *(it + 1) == '/' && *(it - 1) == '/') || (it + 1 == str.cend() && it != str.cbegin() && *(it - 1) == '/') || (it + 1 != str.cend() && it == str.cbegin() && *(it + 1) == '/') || (it + 1 == str.cend() && it == str.cbegin())))
        ret += home;
      else
        ret += *it;
    }
    return ret;
  }

  std::vector<tiny_utf8::string> match_files_and_dirs(const tiny_utf8::string &complete_)
  {
    tiny_utf8::string complete = tilde_expand(complete_);
    std::vector<tiny_utf8::string> ret;
    std::filesystem::directory_iterator dit;
    std::filesystem::path base;
    tiny_utf8::string pattern_to_match;
    auto curr = std::filesystem::current_path();
    if (complete.empty() || complete.find('/') == tiny_utf8::string::npos)
    {
      dit = std::filesystem::directory_iterator{curr};
      pattern_to_match = complete;
      base = std::filesystem::current_path();
    }
    else
    {
      std::filesystem::path path(complete.cpp_str());
      if (std::filesystem::is_directory(path))
      {
        if (!std::filesystem::exists(path))
          return {};
        pattern_to_match = "";
        base = std::filesystem::current_path();
        dit = std::filesystem::directory_iterator{curr / path};
      }
      else
      {
        if (!std::filesystem::exists(path.parent_path()))
          return {};
        pattern_to_match = tilde_expand(complete);
        base = path.parent_path();
        dit = std::filesystem::directory_iterator{path.parent_path()};
      }
    }

    for (auto &dir_entry: dit)
    {
      auto abs = std::filesystem::absolute(dir_entry.path());
      tiny_utf8::string name;
      if (dir_entry.is_directory())
        name = std::filesystem::path(dir_entry.path().string()).filename().string() + "/";
      else
        name = dir_entry.path().filename().string();

      if (begin_with(name, pattern_to_match))
      {
        ret.emplace_back(name);
      }
    }
    return ret;
  }

  size_t display_length(const tiny_utf8::string &str)
  {
    tiny_utf8::string ret;
    for (auto it = str.cbegin(); it < str.cend(); ++it)
    {
      if (*it == '\033')
      {
        while (it < str.cend() && *it != 'm') ++it;
        continue;
      }
      ret += *it;
    }
    return ret.length();
  }

  size_t display_size(const tiny_utf8::string &str)
  {
    size_t size = 0;
    for (auto it = str.cbegin(); it < str.cend(); ++it)
    {
      if (*it == '\033')
      {
        while (it < str.cend() && *it != 'm') ++it;
        continue;
      }
      ++size;
    }
    return size;
  }

  tiny_utf8::string shrink_path(const tiny_utf8::string &path)
  {
    if(path == "/") return "/";
    tiny_utf8::string ret;
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
    if (auto i = path.rfind('/'); i != tiny_utf8::string::npos && i != path.size() - 1)
      ret.insert(ret.size(), path.substr(i + 2));
    return ret;
  }
}