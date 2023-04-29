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
#ifndef DISH_BUILTIN_HPP
#define DISH_BUILTIN_HPP
#pragma once

#include "dish.hpp"

#include <vector>
#include <string>
#include <functional>
#include <map>

namespace dish::builtin
{
  using Args = const std::vector<std::string> &;
  using Func = std::function<int(DishInfo *, Args)>;
  
  int builtin_cd(DishInfo *, Args args);
  
  int builtin_pwd(DishInfo *, Args args);
  
  int builtin_export(DishInfo *, Args args);
  
  int builtin_exit(DishInfo *, Args);
  
  int builtin_history(DishInfo *dish, Args args);
  
  int builtin_help(DishInfo *dish, Args args);
  
  static const std::map<std::string, Func> builtins
      {
          {"cd",      builtin_cd},
          {"pwd",      builtin_pwd},
          {"export",    builtin_export},
          {"exit",    builtin_exit},
          {"history", builtin_history},
          {"help",    builtin_help}
      };
}
#endif