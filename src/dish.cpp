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
#include "error.h"
#include "parser.h"
#include "command.h"
#include "utils.h"
#include <iostream>
#include <unistd.h>
#include <pwd.h>

namespace dish
{
  void Dish::run(const std::string &cmd)
  {
    history.emplace_back(cmd);
    try
    {
      parser::Parser parser{this, cmd};
      parser.parse();
      parser.get_cmd().execute();
    }
    catch (error::Error &err)
    {
      utils::print(err.get_content().c_str());
    }
    catch (error::DishError &err)
    {
      utils::print(err.get_content().c_str());
    }
  }
  
  void Dish::loop()
  {
    while (true)
    {
      std::string cmd;
      //[user@host]
      char hostname[256];
      gethostname(hostname, sizeof(hostname));
      char cwd[256];
      getcwd(cwd, sizeof(cwd));
      auto uid = getuid();
      auto s = utils::colorify(getpwuid(uid)->pw_name, utils::Color::LIGHT_BLUE);
      utils::print("%s Dish %s @ %s in %s \n%s ",
                   utils::colorify("#", utils::Color::LIGHT_BLUE).c_str(),
                   utils::colorify(getpwuid(uid)->pw_name, utils::Color::LIGHT_BLUE).c_str(),
                   utils::colorify(hostname, utils::Color::GREEN).c_str(),
                   utils::colorify(utils::simplify_path(cwd), utils::Color::YELLOW).c_str(),
                   utils::colorify(uid == 0 ? "#" : "$", utils::Color::RED).c_str()
      );
      std::getline(std::cin, cmd);
      if (!cmd.empty())
      {
        run(cmd);
      }
    }
  }
  
  std::vector<std::string> Dish::get_history() const { return history; };
  
}
