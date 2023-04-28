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

#include "dish/error.hpp"
#include "dish/parser.hpp"
#include "dish/lexer.hpp"
#include "dish/command.hpp"
#include "dish/utils.hpp"

#include <iostream>
#include <unistd.h>
#include <pwd.h>

namespace dish
{
  DishInfo::DishInfo(Dish *d) : dish(d), last_ret(0) {}
  
  Dish::Dish() : info(this) {}
  
  void Dish::run(const std::string &cmd)
  {
    history.emplace_back(cmd);
    lexer::Lexer lexer{cmd};
    auto tokens = lexer.get_all_tokens();
    if(!tokens.has_value()) return;
    parser::Parser parser{&info, tokens.value()};
    parser.parse();
    parser.get_cmd().execute();
  }
  
  [[noreturn]] void Dish::loop()
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
      fmt::print("> Dish {}@{}:{} {} \n{} ",
                   utils::blue(getpwuid(uid)->pw_name),
                   utils::green(hostname),
                   utils::yellow(utils::simplify_path(cwd)),
                   info.last_ret == 0 ? "" : utils::red("C: " + std::to_string(info.last_ret)),
                   utils::red(uid == 0 ? "#" : "$"));
      std::getline(std::cin, cmd);
      if (!cmd.empty())
      {
        run(cmd);
      }
    }
  }
  
  std::vector<std::string> Dish::get_history() const { return history; };
  
}
