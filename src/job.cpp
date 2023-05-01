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
#include "dish/job.hpp"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <string>
#include <variant>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <list>
#include <cstdlib>
#include <cstring>

namespace dish::job
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

  void Process::set_job_context(Job *job_)
  {
    job_context = job_;
  }

  void Process::launch()
  {
    int childpid = 0;
    if (builtin::builtins.find(args[0]) != builtin::builtins.end())
    {
      dish_context.last_foreground_ret = builtin::builtins.at(args[0])(args);
      completed = true;
      do_job_notification();
    }
    else
    {
      childpid = fork();
      if (childpid == 0)
      {
        if (dish_context.is_interactive)
        {
          pid_t pid = getpid();
          if (job_context->cmd_pgid == 0)
            job_context->cmd_pgid = pid;
          setpgid(pid, job_context->cmd_pgid);
          if (!job_context->background)
            tcsetpgrp(dish_context.terminal, job_context->cmd_pgid);

          signal(SIGINT, SIG_DFL);
          signal(SIGQUIT, SIG_DFL);
          signal(SIGTSTP, SIG_DFL);
          signal(SIGTTIN, SIG_DFL);
          signal(SIGTTOU, SIG_DFL);
          signal(SIGCHLD, SIG_DFL);
        }
        auto cargs = get_args();
        execv(cmd_path.c_str(), cargs.data());
        fmt::println("execv: {}", strerror(errno));
        std::exit(1);
      }
      else
      {
        pid = childpid;
        if (dish_context.is_interactive)
        {
          if (job_context->cmd_pgid == 0)
            job_context->cmd_pgid = childpid;
          setpgid(childpid, job_context->cmd_pgid);
        }
      }
    }
  }

  void Process::insert(std::string str)
  {
    args.emplace_back(std::move(str));
  }

  bool Process::empty() const
  {
    return args.empty();
  }

  void Process::clear()
  {
    return args.clear();
  }

  std::vector<char *> Process::get_args() const
  {
    std::vector<char *> cargs;
    auto convert = [](const std::string &s) -> char * { return const_cast<char *>(s.c_str()); };
    std::transform(args.begin(), args.end(), std::back_inserter(cargs), convert);
    cargs.emplace_back(static_cast<char *>(nullptr));
    return cargs;
  }

  int Process::find_cmd()
  {
    if (args.empty() || args[0].empty())
    {
      return -1;
    }

    if(builtin::builtins.find(args[0]) != builtin::builtins.end())
    {
      is_builtin = true;
      return 0;
    }
    bool found = false;
    std::string path_to_exe;
    for (auto path: get_path())
    {
      path_to_exe = path + "/" + args[0];
      if (std::filesystem::exists(path_to_exe))
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      fmt::println("dish: command not found: {}", args[0]);
      return -1;
    }
    auto p = std::filesystem::status(path_to_exe).permissions();
    if (!(((p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) ||
         ((p & std::filesystem::perms::group_exec) != std::filesystem::perms::none) ||
         ((p & std::filesystem::perms::others_exec) != std::filesystem::perms::none)))
    {
      fmt::println("dish: permission denied: {}", args[0]);
      return -1;
    }
    cmd_path = path_to_exe;
    return 0;
  }

  Job::Job(std::string cmd)
      : out(RedirectType::fd, 0), in(RedirectType::fd, 1),
        err(RedirectType::fd, 2), background(false), command_str(std::move(cmd)),
        notified(false), cmd_pgid(0)
  {
    tcgetattr (dish_context.terminal, &job_tmodes);
  }

  int Job::launch()
  {
    for(auto& r : processes)
    {
      if(r.find_cmd() != 0)
        return -1;
    }
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

    for (auto it = processes.begin(); it < processes.end(); ++it)
    {
      auto &scmd = *it;
      dup2(fdin, 0);
      close(fdin);
      if (it + 1 == processes.cend())
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
      scmd.launch();
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
    if (!dish_context.is_interactive)
      wait();
    else if (!background)
      put_in_foreground(0);
    else
    {
      fmt::println(format_job_info("launched"));
      put_in_background(0);
    }
    return 0;
  }

  void Job::put_in_foreground(int cont)
  {
    tcsetpgrp(dish_context.terminal, cmd_pgid);
    if (cont)
    {
      tcsetattr(dish_context.terminal, TCSADRAIN, &job_tmodes);
      if (kill(-cmd_pgid, SIGCONT) < 0)
        fmt::println("kill (SIGCONT)");
    }

    wait();

    //Put Dish in the foreground.
    tcsetpgrp(dish_context.terminal, dish_context.pgid);

    tcgetattr(dish_context.terminal, &job_tmodes);
    tcsetattr(dish_context.terminal, TCSADRAIN, &dish_context.tmodes);
  }

  void Job::put_in_background(int cont)
  {
    if (cont)
    {
      if (kill(-cmd_pgid, SIGCONT) < 0)
        fmt::println("kill (SIGCONT): {}", strerror(errno));
    }
  }

  void Job::insert(const Process &scmd)
  {
    processes.emplace_back(std::move(scmd));
    processes.back().set_job_context(this);
  }

  void Job::set_in(Redirect redirect) { in = std::move(redirect); }

  void Job::set_out(Redirect redirect) { out = std::move(redirect); }

  void Job::set_err(Redirect redirect) { err = std::move(redirect); }

  void Job::set_background()
  {
    background = true;
  }

  void Job::set_foreground()
  {
    background = false;
  }

  bool Job::is_stopped()
  {
    for (auto &p: processes)
    {
      if (!p.completed && !p.stopped)
        return false;
    }
    return true;
  }

  bool Job::is_background()
  {
    return background;
  }

  bool Job::is_completed()
  {
    for (auto &p: processes)
    {
      if (!p.completed)
        return false;
    }
    return true;
  }

  bool Job::is_builtin()
  {
    for (auto &p: processes)
    {
      if (!p.is_builtin)
        return false;
    }
    return true;
  }

  int Job::mark_status(int pid, int status)
  {
    if (pid > 0)
    {
      for (auto &p: processes)
      {
        if (p.pid == pid)
        {
          p.status = status;
          if (WIFSTOPPED(status))
            p.stopped = true;
          else
          {
            p.completed = true;
            if (WIFSIGNALED(status))
              fmt::println("{}: Terminated by signal {}.", pid, WTERMSIG(p.status));
            else if (WIFEXITED(status))
            {
              auto es = WEXITSTATUS(status);
              p.exit_status = es;
              if (!is_background())
                dish_context.last_foreground_ret = es;
            }
          }
          return 0;
        }
      }
      fmt::println("mark_status: No such child process {}.", pid);
      return -1;
    }
    else if (pid == 0 || errno == ECHILD)
      return -1;
    fmt::println("waitpid: {}", strerror(errno));
    return -1;
  }

  void Job::wait()
  {
    if (!is_builtin())
    {
      int status;
      pid_t pid;
      dish_context.waiting = true;
      do
        pid = waitpid(-1, &status, WUNTRACED);
      while (!mark_status(pid, status) && !is_stopped() && !is_completed());
      dish_context.waiting = false;
    }
    do_job_notification();
  }

  void Job::update_status()
  {
    if(!is_builtin())
    {
      int status;
      pid_t pid;
      do
        pid = waitpid(-1, &status, WUNTRACED|WNOHANG);
      while(!mark_status(pid, status)&& !is_stopped() && !is_completed());
    }
  }

  [[nodiscard]]std::string Job::format_job_info(const std::string &status)
  {
    return fmt::format("{} [{}]: {}", cmd_pgid, status, command_str);
  }

  void Job::continue_job()
  {
    for (auto &p: processes)
      p.stopped = false;
    notified = 0;
    if (!background)
      put_in_foreground (1);
    else
      put_in_background (1);
  }
}