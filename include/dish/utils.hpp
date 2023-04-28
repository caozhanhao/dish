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

#include "error.hpp"

#define FMT_HEADER_ONLY
#include "bundled/fmt/core.h"

#include <sys/types.h>
#include <dirent.h>

#include <vector>
#include <algorithm>
#include <string>
#include <regex>
#include <cstdio>
#include <cstring>

namespace dish::utils
{
  enum class Color : std::size_t
  {
    RED = 31, GREEN, YELLOW, BLUE, PURPLE, LIGHT_BLUE,
    WHITE
  };
  
  static std::string colorify(const std::string &str, Color type)
  {
    return "\033[1;" + std::to_string(static_cast<std::size_t>(type)) + "m" + str + "\033[0m";
  }
  
  static std::string red(const std::string &str)
  {
    return colorify(str, Color::RED);
  }
  static std::string green(const std::string &str)
  {
    return colorify(str, Color::GREEN);
  }
  static std::string yellow(const std::string &str)
  {
    return colorify(str, Color::YELLOW);
  }
  static std::string blue(const std::string &str)
  {
    return colorify(str, Color::BLUE);
  }
  static std::string purple(const std::string &str)
  {
    return colorify(str, Color::PURPLE);
  }
  static std::string light_blue(const std::string &str)
  {
    return colorify(str, Color::BLUE);
  }
  static std::string white(const std::string &str)
  {
    return colorify(str, Color::WHITE);
  }
  
  static bool has_wildcards(const std::string &s)
  {
    return (s.find('*') != std::string::npos ||
            s.find('?') != std::string::npos);
  }
  
  static std::vector<std::string> expand_wildcards(const std::string &s)
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
      throw error::DishError(DISH_ERROR_LOCATION, __func__, std::string("opendir :") + strerror(errno));
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
        } else
        {
          ret.emplace_back(ent->d_name);
        }
      }
    }
    closedir(dir);
    std::sort(ret.begin(), ret.end());
    return ret;
  }
  
  static std::string get_home()
  {
    char const *home = getenv("HOME");
    if (home == nullptr) home = getenv("USERPROFILE");
    if (home == nullptr) home = getenv("HOMEDRIVE");
    if (home == nullptr) home = getenv("HOMEPATH");
    if (home == nullptr) return "";
    return home;
  }
  
  static std::string simplify_path(const std::string &path)
  {
    std::string home = get_home();
    if (home.empty()) return path;
    if (home.back() == '/') home.pop_back();
    auto [it1, it2] = std::mismatch(path.cbegin(), path.cend(), home.cbegin(), home.cend());
    if (it2 == home.cend())
    {
      return "~" + path.substr(home.size());
    }
    return path;
  }
}
#endif