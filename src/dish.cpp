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

#include "dish/parser.hpp"
#include "dish/lexer.hpp"
#include "dish/job.hpp"
#include "dish/utils.hpp"

#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>

#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <filesystem>

extern char ** environ;

namespace dish
{
  dish::DishContext dish_context;

  auto to_token(const std::string& cmd)
  {
    return lexer::Lexer(cmd).get_all_tokens_no_check().value();
  }
  void sigchld_handler(int sig)
  {
    if(!dish_context.waiting)
      do_job_notification();
  }

  void init()
  {
    dish_context.last_foreground_ret = 0;
    dish_context.last_dir = "/";
    dish_context.waiting = false;

    dish_context.terminal = STDIN_FILENO;
    dish_context.is_interactive = isatty(dish_context.terminal);

    dish_context.alias = {
            {"ls", "ls --color=tty"},
            {"grep", "grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"}
    };

    char ** envir = environ;
    while(*envir)
    {
      std::string tmp{*envir};
      auto eq = tmp.find('=');
      if(eq == std::string::npos)
      {
        fmt::println("Unexpected env: {}", tmp);
        std::exit(-1);
      }
      dish_context.env[tmp.substr(0, eq)] = tmp.substr(eq + 1);
      envir++;
    }

    dish_context.env["PWD"] = std::filesystem::current_path().string();
    dish_context.env["HOME"] = getpwuid(getuid())->pw_dir;

    if(dish_context.env.find("PATH") == dish_context.env.end())
      dish_context.env["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";

    if (dish_context.is_interactive)
    {
      while (tcgetpgrp(dish_context.terminal) != (dish_context.pgid = getpgrp()))
        kill(-dish_context.pgid, SIGTTIN);
      
      signal(SIGINT, SIG_IGN);
      signal(SIGQUIT, SIG_IGN);
      signal(SIGTSTP, SIG_IGN);
      signal(SIGTTIN, SIG_IGN);
      signal(SIGTTOU, SIG_IGN);
      signal(SIGCHLD, sigchld_handler);

      dish_context.pgid = getpid();
      if (setpgid(dish_context.pgid, dish_context.pgid) < 0)
      {
        fmt::println("DishError: Failed to put the shell in its own process group.");
        //std::exit(1);
      }
      tcsetpgrp(dish_context.terminal, dish_context.pgid);
      tcgetattr(dish_context.terminal, &dish_context.tmodes);
    }
  }
  
  void run(const std::string &cmd)
  {
    dish_context.history.emplace_back(cmd);
    lexer::Lexer lexer{cmd};
    auto tokens = lexer.get_all_tokens();
    if (!tokens.has_value()) return;
    parser::Parser parser{cmd, tokens.value()};
    if (parser.parse() == -1) return;
    dish_context.jobs.emplace_back(std::make_shared<job::Job>(parser.get_cmd()));
    dish_context.jobs.back()->launch();
  }
  
  void do_job_notification()
  {
    for (auto job_it = dish_context.jobs.begin(); job_it != dish_context.jobs.end();)
    {
      auto &job = *job_it;
      job->update_status();
      if (job->is_completed())
      {
        if(job->is_background())
          fmt::println("\n{}", job->format_job_info("completed"));
        job_it = dish_context.jobs.erase(job_it);
      }
      else if (job->is_stopped() && !job->notified)
      {
        if(job->is_background())
          fmt::println("\n{}", job->format_job_info("stopped"));
        job->notified = true;
        ++job_it;
      }
      else
        ++job_it;
    }
    return;
  }

  std::vector<std::string> get_path()
  {
    std::vector<std::string> ret;
    if(auto it = dish_context.env.find("PATH"); it != dish_context.env.end())
      ret = utils::split<std::vector<std::string>>(it->second, ":");
    ret.emplace_back(std::filesystem::current_path().string());
    return ret;
  }

  [[noreturn]]void loop()
  {
    while (true)
    {
      std::string cmd;
      char hostname[256];
      gethostname(hostname, sizeof(hostname));
      auto uid = getuid();
      fmt::print("\n> Dish {}@{}:{} {} \n{} ",
                 utils::blue(getpwuid(uid)->pw_name),
                 utils::green(hostname),
                 utils::yellow(utils::simplify_path(std::filesystem::current_path().string())),
                 dish_context.last_foreground_ret == 0 ? "" : utils::red("C: " + std::to_string(dish_context.last_foreground_ret)),
                 utils::red(uid == 0 ? "#" : "$"));

      std::cin.clear();
      std::getline(std::cin, cmd);
      if (!cmd.empty())
      {
        run(cmd);
      }
    }
  }
}
