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
#include <sys/wait.h>
#include <fcntl.h>

namespace dish::cmd
{
  //Redirect
  bool Redirect::is_description() const { return redirect.index() == 0; }
  
  std::string Redirect::get_filename() const { return std::get<std::string>(redirect); }
  
  int Redirect::get_description() const { return std::get<int>(redirect); }
  
  int Redirect::get(int mode) const
  {
    if (is_description())
    {
      return get_description();
    } else
    {
      return open(get_filename().c_str(), mode);
    }
  }
  
  //SingleCmd
  void SingleCmd::set_info(DishInfo *dish_)
  {
    info = dish_;
  }
  
  //SimpleCmd
  int SimpleCmd::execute()
  {
    int ret = 0;
    expand_wildcards();
    if (builtin::builtins.find(args[0]) != builtin::builtins.end())
      builtin::builtins.at(args[0])(info, args);
    else
    {
      int childpid = fork();
      if (childpid == 0)
      {
        auto cargs = get_cargs();
        cargs.emplace_back(static_cast<char *>(nullptr));
        if (execvp(cargs[0], cargs.data()) == -1)
        {
          throw error::DishError(DISH_ERROR_LOCATION, __func__, std::string("execvp :") + strerror(errno));
        }
        std::exit(1);
      } else
      {
        int child_status;
        if (!info->background) waitpid(childpid, &child_status, 0);
        ret = WEXITSTATUS(child_status);
      }
    }
    info->last_ret = ret;
    return ret;
  }
  
  void SimpleCmd::insert(std::string str)
  {
    args.emplace_back(std::move(str));
  }
  
  void SimpleCmd::expand_wildcards()
  {
    for (auto it = args.begin(); it < args.end();)
    {
      if (utils::has_wildcards(*it))
      {
        auto expanded = utils::expand_wildcards(*it);
        it = args.erase(it);
        it = args.insert(it, std::make_move_iterator(expanded.begin()),
                         std::make_move_iterator(expanded.end()));
      } else
      {
        ++it;
      }
    }
  }
  
  std::vector<char *> SimpleCmd::get_cargs() const
  {
    std::vector<char *> vc;
    auto convert = [](const std::string &s) -> char *
    {
      return const_cast<char *>(s.c_str());
    };
    std::transform(args.begin(), args.end(), std::back_inserter(vc), convert);
    return vc;
  }
  
  //IfCmd
  int IfCmd::execute()
  {
    if (condition->execute() == 0)
      return true_case->execute();
    return false_case->execute();
  }
  
  //Command
  Command::Command(DishInfo *info_)
      : out(RedirectType::output, 0), in(RedirectType::input, 1),
        err(RedirectType::output, 0), background(false), info(info_) {}
  
  int Command::execute()
  {
    int tmpin = dup(0);
    int tmpout = dup(1);
    int fdin = 0;
    if (!in.is_description() || in.get_description() != 1)
    {
      fdin = in.get(O_RDONLY);
    } else
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
        } else
        {
          fdout = dup(tmpout);
        }
      } else
      {
        int fdpipe[2];
        pipe(fdpipe);
        fdout = fdpipe[1];
        fdin = fdpipe[0];
      }
      
      info->background = background;
      ret = scmd->execute();
      
      dup2(fdout, 1);
      close(fdout);
    }
    
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    
    close(tmpin);
    close(tmpout);
    return ret;
  }
  
  void Command::insert(std::shared_ptr<SingleCmd> scmd)
  {
    commands.emplace_back(std::move(scmd));
    commands.back()->set_info(info);
  }
  
  void Command::set_in(Redirect redirect) { in = std::move(redirect); }
  
  void Command::set_out(Redirect redirect) { out = std::move(redirect); }
  
  void Command::set_err(Redirect redirect) { err = std::move(redirect); }
  
  void Command::set_info(DishInfo *dish_) { info = dish_; }
  
  void Command::set_background() { background = true; }
}
