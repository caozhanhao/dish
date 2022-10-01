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
#include "builtin.h"
#include "command.h"
#include <string>
#include <variant>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

namespace dish::cmd
{
  bool Redirect::is_description() const { return redirect.index() == 0; }
  
  std::string Redirect::get_filename() const { return std::get<std::string>(redirect); }
  
  int Redirect::get_description() const { return std::get<int>(redirect); }
  
  int Redirect::get(int mode) const
  {
    if (is_description())
    {
      return get_description();
    }
    else
    {
      return open(get_filename().c_str(), mode);
    }
  }
  
  void SingleCommand::insert(std::string str)
  {
    args.emplace_back(std::move(str));
  }
  
  void SingleCommand::expand_wildcards()
  {
    for (auto it = args.begin(); it < args.end();)
    {
      if (utils::has_wildcards(*it))
      {
        auto expanded = utils::expand_wildcards(*it);
        it = args.erase(it);
        it = args.insert(it, std::make_move_iterator(expanded.begin()),
                         std::make_move_iterator(expanded.end()));
      }
      else
      {
        ++it;
      }
    }
  }
  
  const std::vector<std::string> &SingleCommand::get_args() const
  {
    return args;
  }
  
  std::vector<char *> SingleCommand::get_cargs() const
  {
    std::vector<char *> vc;
    auto convert = [](const std::string &s) -> char *
    {
      return const_cast<char *>(s.c_str());
    };
    std::transform(args.begin(), args.end(), std::back_inserter(vc), convert);
    return vc;
  }
  
  void SingleCommand::clear()
  {
    args.clear();
  }
  
  bool SingleCommand::empty() const
  {
    return args.empty();
  }
  
  Command::Command()
      : out(RedirectType::output, 0), in(RedirectType::input, 1),
        err(RedirectType::output, 0), background(false), dish(
          nullptr) {}
  
  void Command::execute()
  {
    int tmpin = dup(0);
    int tmpout = dup(1);
    int fdin = 0;
    if (!in.is_description() || in.get_description() != 1)
    {
      fdin = in.get(O_RDONLY);
    }
    else
    {
      fdin = dup(tmpin);
    }
    
    int ret = 0;
    for (auto it = commands.begin(); it < commands.end(); ++it)
    {
      auto &scmd = *it;
      dup2(fdin, 0);
      close(fdin);
      int fdout = 0;
      if (it + 1 == commands.cend())
      {
        if (!out.is_description() || out.get_description() != 0)
        {
          fdout = out.get(O_WRONLY);
        }
        else
        {
          fdout = dup(tmpout);
        }
      }
      else
      {
        int fdpipe[2];
        pipe(fdpipe);
        fdout = fdpipe[1];
        fdin = fdpipe[0];
      }
      
      dup2(fdout, 1);
      close(fdout);
      
      scmd.expand_wildcards();
      auto &args = scmd.get_args();
      if (builtin::builtins.find(args[0]) != builtin::builtins.end())
      {
        builtin::builtins.at(args[0])(dish, args);
      }
      else
      {
        ret = fork();
        if (ret == 0)
        {
          auto cargs = scmd.get_cargs();
          cargs.emplace_back(static_cast<char *>(0));
          if (execvp(cargs[0], cargs.data()) != 0)
          {
            throw error::DishError(DISH_ERROR_LOCATION, __func__, std::string("execvp :") + strerror(errno));
          }
          std::exit(1);
        }
      }
    }
    
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    
    close(tmpin);
    close(tmpout);
    if (!background) waitpid(ret, NULL, 0);
  }
  
  void Command::insert(const SingleCommand &scmd) { commands.emplace_back(scmd); }
  
  void Command::set_in(Redirect redirect) { in = std::move(redirect); }
  
  void Command::set_out(Redirect redirect) { out = std::move(redirect); }
  
  void Command::set_err(Redirect redirect) { err = std::move(redirect); }
  
  void Command::set_dish(Dish *dish_) { dish = dish_; }
  
  void Command::set_background() { background = true; }
}
