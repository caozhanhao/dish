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

namespace dish::utils
{
  std::string effect(const std::string &str, Effect effect_)
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

  std::string red(const std::string &str)
  {
    return effect(str, Effect::fg_red);
  }
  std::string green(const std::string &str)
  {
    return effect(str, Effect::fg_green);
  }
  std::string yellow(const std::string &str)
  {
    return effect(str, Effect::fg_yellow);
  }
  std::string blue(const std::string &str)
  {
    return effect(str, Effect::fg_blue);
  }
  std::string magenta(const std::string &str)
  {
    return effect(str, Effect::fg_magenta);
  }
  std::string cyan(const std::string &str)
  {
    return effect(str, Effect::fg_cyan);
  }
  std::string white(const std::string &str)
  {
    return effect(str, Effect::fg_white);
  }

  std::string get_dish_env(const std::string &s)
  {
    if (auto it = dish_context.lua_state["dish"]["environment"][s]; it.valid())
      return it.get<std::string>();
    return "";
  }
  bool has_wildcards(const std::string &s)
  {
    return (s.find('*') != std::string::npos ||
            s.find('?') != std::string::npos);
  }

  std::optional<std::string> get_home()
  {
    std::string home;
    const std::vector<std::string> env_to_find{"HOME", "USERPROFILE", "HOMEDRIVE", "HOMEPATH"};
    for (auto &r: env_to_find)
    {
      if (auto it = dish_context.lua_state["dish"]["environment"][r]; it.valid())
      {
        home = it.get<std::string>();
        break;
      }
    }
    if (home.empty()) return std::nullopt;
    if (*home.rbegin() == '/') home.pop_back();
    return home;
  }

  std::optional<std::vector<std::string>> expand(const std::string &str)
  {
    std::string s;
    auto home = get_home();
    // Tilde(~)
    if (home.has_value())
    {
      for (auto it = str.cbegin(); it < str.cend(); ++it)
      {
        if (*it == '~' &&
            ((it + 1 != str.cend() && it != str.cbegin() && *(it + 1) == '/' && *(it - 1) == '/') || (it + 1 == str.cend() && it != str.cbegin() && *(it - 1) == '/') || (it + 1 != str.cend() && it == str.cbegin() && *(it + 1) == '/') || (it + 1 == str.cend() && it == str.cbegin())))
        {
          s += home.value();
        }
        else
          s += *it;
      }
    }
    else
      s = str;
    // Wildcards
    if (utils::has_wildcards(str))
    {
      std::string pattern{'^'};
      for (auto it = s.cbegin(); it < s.cend(); ++it)
      {
        auto &ch = *it;
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

      std::regex re(pattern);
      std::vector<std::string> ret;
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

  std::string get_timestamp()
  {
    auto tp = std::chrono::time_point_cast<std::chrono::seconds>(std::chrono::system_clock::now());
    return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count());
  }

  std::string to_string(CommandType ct)
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

  bool begin_with(const std::string &a, const std::string &b)
  {
    if (a.size() < b.size()) return false;
    for (size_t i = 0; i < b.size(); ++i)
    {
      if (a[i] != b[i])
        return false;
    }
    return true;
  }

  std::set<Command> match_command(const std::string &pattern)
  {
    std::set<Command> ret;
    // PATH
    for (auto &path: get_path())
    {
      for (auto &dir_entry: std::filesystem::directory_iterator{path})
      {
        if (dir_entry.is_directory() || !is_executable(dir_entry.path())) continue;
        if (begin_with(dir_entry.path().filename().string(), pattern))
        {
          auto size = std::filesystem::file_size(dir_entry.path());
          CommandType type = CommandType::executable_file;
          if (std::filesystem::is_symlink(dir_entry.path()))
            type = CommandType::executable_link;
          ret.insert(Command{dir_entry.path().filename(), type, size});
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
      auto fn = r.first.as<std::string>();
      if (begin_with(fn, pattern))
        ret.insert(Command{fn, CommandType::lua_func, 0});
    }
    return ret;
  }

  std::tuple<CommandType, std::string> find_command(const std::string &cmd)
  {
    if (builtin::builtins.find(cmd) != builtin::builtins.end())
      return {CommandType::builtin, cmd};

    if (dish_context.lua_state["dish"]["func"][cmd].valid())
      return {CommandType::lua_func, cmd};

    bool found = false;
    std::string abs_path;
    for (auto path: get_path())
    {
      abs_path = path + "/" + cmd;
      if (std::filesystem::exists(abs_path))
      {
        found = true;
        break;
      }
    }
    if (!found)
      return {CommandType::not_found, ""};
    if (!is_executable(abs_path))
      return {CommandType::not_executable, abs_path};
    if (std::filesystem::is_symlink(std::filesystem::path(abs_path)))
      return {CommandType::executable_link, abs_path};
    return {CommandType::executable_file, abs_path};
  }

  std::string get_human_readable_size(size_t sz)
  {
    int i = 0;
    double mantissa = sz;
    for (; mantissa >= 1024.; mantissa /= 1024., ++i) {
    }
    mantissa = std::ceil(mantissa * 10.) / 10.;
    return fmt::format("{}{}", mantissa, "BKMGTPE"[i]);
  }

  std::string simplify_path(const std::string &path_)
  {
    std::string path = std::filesystem::path(path_).lexically_normal();
    auto home_opt = get_home();
    if (!home_opt.has_value()) return path;
    auto home = home_opt.value();
    auto [it1, it2] = std::mismatch(path.cbegin(), path.cend(), home.cbegin(), home.cend());
    if (it2 == home.cend())
      return "~" + path.substr(home.size());
    return path;
  }


  std::vector<std::string> match_files_and_dirs(const std::string &complete)
  {
    std::vector<std::string> ret;
    std::filesystem::directory_iterator dit;
    std::string pattern_to_match;
    auto curr = std::filesystem::current_path();
    if (complete.empty() || complete.find('/') == std::string::npos)
    {
      dit = std::filesystem::directory_iterator{curr};
      pattern_to_match = complete;
    }
    else
    {
      std::filesystem::path path(complete);
      if (std::filesystem::is_directory(path))
      {
        if (!std::filesystem::exists(path))
          return {};
        pattern_to_match = "";
        dit = std::filesystem::directory_iterator{curr / path};
      }
      else
      {
        if (!std::filesystem::exists(path.parent_path()))
          return {};
        pattern_to_match = complete;
        dit = std::filesystem::directory_iterator{path.parent_path()};
      }
    }

    for (auto &dir_entry: dit)
    {
      if (begin_with(dir_entry.path().lexically_relative(curr).string(),
                     pattern_to_match))
      {
        ret.emplace_back(dir_entry.path().lexically_relative(curr).lexically_normal()
                                 .string());
        if (dir_entry.is_directory())
          ret.back() += "/";
      }
    }
    return ret;
  }

  size_t get_length_without_ansi_escape(const std::string &str)
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

  std::string shrink_path(const std::string &path)
  {
    if(path == "/") return "/";
    std::string ret;
    if (path[0] != '/')
      ret += path[0];
    ret += "/";

    for (auto it = path.cbegin(); it < path.cend(); ++it)
    {
      if (it - 1 >= path.cbegin() && *(it - 1) == '/')
      {
        ret += *it;
        ret += "/";
      }
    }
    ret.pop_back();
    if (auto i = path.rfind('/'); i != std::string::npos && i != path.size() - 1)
      ret.insert(ret.size(), path.substr(i + 2));
    return ret;
  }
}