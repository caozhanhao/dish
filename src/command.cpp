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
#include "dish/builtin.hpp"
#include "dish/command.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <string>
#include <variant>
#include <algorithm>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstring>

namespace dish::cmd
{
  //Redirect
  bool Redirect::is_description() const { return redirect.index() == 0; }
  
  std::string Redirect::get_filename() const { return std::get<std::string>(redirect); }
  
  int Redirect::get_description() const { return std::get<int>(redirect); }
  
  int Redirect::get() const
  {
    switch (type)
    {
      case RedirectType::input:
        return open(get_filename().c_str(), O_RDONLY);
        break;
      case RedirectType::overwrite:
        return open(get_filename().c_str(), O_CREAT | O_TRUNC | O_WRONLY);
        break;
      case RedirectType::append:
        return open(get_filename().c_str(), O_CREAT | O_APPEND | O_WRONLY);
        break;
      case RedirectType::fd:
        return dup(get_description());
        break;
    }
    return -1;
  }
  
  //SingleCmd
  void SingleCmd::set_info(DishInfo *dish_)
  {
    info = dish_;
  }
  
  //SimpleCmd
  std::pair<int, int> SimpleCmd::execute()
  {
    int ret, childpid = 0;
    if (builtin::builtins.find(args[0]) != builtin::builtins.end())
    {
      ret = builtin::builtins.at(args[0])(info, args);
    }
    else
    {
      childpid = fork();
      if (childpid == 0)
      {
        auto cargs = get_cargs();
        cargs.emplace_back(static_cast<char *>(nullptr));
        execvp(cargs[0], cargs.data());
        fmt::println("execvp: {}", strerror(errno));
        std::exit(1);
      }
      else
      {
        int child_status;
        ret = WEXITSTATUS(child_status);
      }
    }
    info->last_ret = ret;
    return {childpid, ret};
  }
  
  void SimpleCmd::insert(std::string str)
  {
    args.emplace_back(std::move(str));
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
  
  //Command
  Command::Command(DishInfo *info_)
      : out(RedirectType::fd, 0), in(RedirectType::fd, 1),
        err(RedirectType::fd, 2), background(false), info(info_) {}
  
  int Command::execute()
  {
    int tmpin = dup(0);
    int tmpout = dup(1);
    int fdin = 0;
    int fdout = 0;
    if (!in.is_description() || in.get_description() != 1)
    {
      fdin = in.get();
      if (fdin == -1)
      {
        fmt::println("open/dup: {}", strerror(errno));
        return -1;
      }
    }
    else
    {
      fdin = dup(tmpin);
      if (fdin == -1)
      {
        fmt::println("dup: {}", strerror(errno));
        return -1;
      }
    }
  
    int ret, pid = 0;
    for (auto it = commands.begin(); it < commands.end(); ++it)
    {
      auto &scmd = *it;
      dup2(fdin, 0);
      close(fdin);
      if (it + 1 == commands.cend())
      {
        if (!out.is_description() || out.get_description() != 0)
        {
          fdout = out.get();
          if (fdout == -1)
          {
            fmt::println("open/dup: {}", strerror(errno));
            return -1;
          }
        }
        else
        {
          fdout = dup(tmpout);
          if (fdout == -1)
          {
            fmt::println("dup: {}", strerror(errno));
            return -1;
          }
        }
      }
      else
      {
        int fdpipe[2];
        if (pipe(fdpipe) == -1)
        {
          fmt::println("pipe: {}", strerror(errno));
          return -1;
        }
        fdout = fdpipe[1];
        fdin = fdpipe[0];
      }
      if (dup2(fdout, 1) == -1)
      {
        fmt::println("dup2: {}", strerror(errno));
        return -1;
      }
      if (close(fdout) == -1)
      {
        fmt::println("close: {}", strerror(errno));
        return -1;
      }
      info->background = background;
      
      auto ret_pair = scmd->execute();
      ret = ret_pair.second;
      pid = ret_pair.first;
    }
  
    if (dup2(tmpin, 0) == -1)
    {
      fmt::println("dup2: {}", strerror(errno));
      return -1;
    }
    if (dup2(tmpout, 1) == -1)
    {
      fmt::println("dup2: {}", strerror(errno));
      return -1;
    }
  
    if (close(tmpin) == -1)
    {
      fmt::println("close: {}", strerror(errno));
      return -1;
    }
    if (close(tmpout) == -1)
    {
      fmt::println("close: {}", strerror(errno));
      return -1;
    }
    if (!info->background) waitpid(pid, nullptr, 0);
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
