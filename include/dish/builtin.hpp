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

#include "type_alias.hpp"
#include "utils.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace dish::builtin
{
  using Args = const std::vector<String> &;
  using Func = std::function<int(Args)>;

  int builtin_cd(Args);

  int builtin_pwd(Args);

  int builtin_export(Args);

  int builtin_unset(Args);

  int builtin_exit(Args);

  int builtin_history(Args);

  int builtin_help(Args);

  int builtin_jobs(Args);

  int builtin_fg(Args);

  int builtin_bg(Args);

  int builtin_alias(Args);

  int builtin_type(Args);

  int builtin_source(Args);

  static const std::map<String, Func> builtins{
          {"cd", builtin_cd},
          {"pwd", builtin_pwd},
          {"export", builtin_export},
          {"unset", builtin_unset},
          {"exit", builtin_exit},
          {"history", builtin_history},
          {"help", builtin_help},
          {"jobs", builtin_jobs},
          {"fg", builtin_fg},
          {"bg", builtin_bg},
          {"alias", builtin_alias},
          {"type", builtin_type},
          {"source", builtin_source},
  };
}// namespace dish::builtin
#endif