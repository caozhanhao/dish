//   Copyright 2022 dish - caozhanhao
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
#include "utils.h"
#include "error.h"
#include "dish.h"
#include "builtin.h"
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
      throw error::Error("Program Error", "cd: Unexpected arguments.", utils::mark_error(args, 2));
    } else if (args.size() == 1)
    {
      auto home = utils::get_home();
      if (home.empty())
      {
        throw error::Error("Program Error", "cd: Can not find ~", utils::mark_error(args, 0));
      }
      if (chdir(home.c_str()) != 0)
      {
        throw error::Error("Program Error", std::string("cd: ") + strerror(errno), utils::mark_error(args, 0));
      }
    } else
    {
      std::string arg;
      if (args[1][0] == '~')
      {
        arg = utils::get_home();
        if (arg.empty())
        {
          throw error::Error("Program Error", "cd: Can not find ~", utils::mark_error(args, 0));
        }
        if (arg.back() == '/') arg.pop_back();
        arg += args[1].substr(1);
      } else
      {
        arg = args[1];
      }
      if (chdir(arg.c_str()) != 0)
      {
        throw error::Error("Program Error", std::string("cd: ") + strerror(errno), utils::mark_error(args, 1));
      }
    }
    return 1;
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
      utils::print("%d| %s\n", index, r.c_str());
      index++;
    }
    return 1;
  }
  
  int builtin_help(DishInfo *dish, Args args)
  {
    std::string list;
    for (auto &r:builtins)
    {
      list += utils::colorify(r.first, utils::Color::LIGHT_BLUE);
      list += ", ";
    }
    if (!list.empty())
    {
      list.pop_back();
      list.pop_back();
      list += "\n";
    }
    utils::print("Dish - caozhanhao, version 0.0.1-beta\n"
                 "These are builtin-commands. Use 'help' to see this.\n"
                 + list
    );
    return 1;
  }
}
