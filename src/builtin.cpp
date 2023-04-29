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
#include "dish/error.hpp"
#include "dish/dish.hpp"
#include "dish/builtin.hpp"

#include <map>
#include <vector>
#include <string>
#include <functional>
#include <unistd.h>

namespace dish::builtin
{
  int builtin_cd(DishInfo *, Args args)
  {
    if (args.size() > 2)
    {
      fmt::println("cd: too many arguments.");
      return -1;
    }
    else if (args.size() == 1)
    {
      auto home_opt = utils::get_home();
      if (!home_opt.has_value())
      {
        fmt::println("cd: Can not find ~");
        return -1;
      }
      if (chdir(home_opt.value().c_str()) != 0)
      {
        fmt::println("cd: {}", strerror(errno));
        return -1;
      }
    } else
    {
      std::string arg;
      if (args[1][0] == '~')
      {
        auto home_opt = utils::get_home();
        if (!home_opt.has_value())
        {
          fmt::println("cd: Can not find ~");
          return -1;
        }
        arg = home_opt.value();
        if (arg.back() == '/') arg.pop_back();
        arg += args[1].substr(1);
      } else
      {
        arg = args[1];
      }
      if (chdir(arg.c_str()) != 0)
      {
        fmt::println("cd: {}", strerror(errno));
        return -1;
      }
    }
    return 1;
  }
  int builtin_pwd(DishInfo *, Args args)
  {
    if (args.size() != 1)
    {
      fmt::println("pwd: too many arguments.");
      return -1;
    }
    else
    {
      auto cwd = utils::get_working_directory();
      if(!cwd.has_value())
      {
        fmt::println("pwd: {}", strerror(errno));
        return -1;
      }
      fmt::println(cwd.value());
    }
    return 1;
  }
  
  int builtin_export(DishInfo *, Args)
  {
  
  }
  
  int builtin_exit(DishInfo *, Args)
  {
    std::exit(0);
  }
  
  int builtin_history(DishInfo *dish, Args args)
  {
    int index = 1;
    for (auto &r:dish->dish->get_history())
    {
      fmt::println("{}| {}", index, r);
      index++;
    }
    return 1;
  }
  
  int builtin_help(DishInfo *dish, Args args)
  {
    std::vector<std::string> list;
    for (auto &r:builtins)
      list.emplace_back(utils::light_blue(r.first));
    
    fmt::println("{} \n {} \n {}",
        "Dish - caozhanhao, version 0.0.1-beta.",
        "These are builtin-commands. Use 'help' to see this.",
        fmt::join(list, ", ")
    );
    return 1;
  }
}
