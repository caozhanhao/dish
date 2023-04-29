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

namespace dish
{
  std::vector<std::string> dish_history;
  dish::DishContext dish_context;
  std::vector<std::shared_ptr<dish::job::Job>> dish_jobs;
  
  void init()
  {
    dish_context.last_ret = 0;
    dish_context.last_dir = "/";
  
    dish_context.terminal = STDIN_FILENO;
    dish_context.is_interactive = isatty(dish_context.terminal);
    
    if (dish_context.is_interactive)
    {
      while (tcgetpgrp(dish_context.terminal) != (dish_context.pgid = getpgrp()))
        kill(-dish_context.pgid, SIGTTIN);
      
      signal(SIGINT, SIG_IGN);
      signal(SIGQUIT, SIG_IGN);
      signal(SIGTSTP, SIG_IGN);
      signal(SIGTTIN, SIG_IGN);
      signal(SIGTTOU, SIG_IGN);
      
      struct sigaction sigchild;
      sigemptyset(&sigchild.sa_mask);
      sigchild.sa_flags = 0;
      sigchild.sa_handler = do_job_notification;
      sigaction(SIGCHLD, &sigchild, nullptr);
  
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
    dish_history.emplace_back(cmd);
    lexer::Lexer lexer{cmd};
    auto tokens = lexer.get_all_tokens();
    if (!tokens.has_value()) return;
    parser::Parser parser{cmd, tokens.value()};
    if (parser.parse() == -1) return;
    dish_jobs.emplace_back(std::make_shared<job::Job>(parser.get_cmd()));
    dish_jobs.back()->launch();
  }
  
  int mark_process_status(int pid, int status)
  {
    if (pid > 0)
    {
      for (auto &job : dish_jobs)
      {
        for (auto &p: job->processes)
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
                fmt::println("{}: Terminated by signal {}.", pid, WTERMSIG (p.status));
            }
            return 0;
          }
        }
        fmt::println("No child process {}.", pid);
        return -1;
      }
    }
    else if (pid == 0 || errno == ECHILD)
      return -1;
    fmt::println("waitpid: {}", strerror(errno));
    return -1;
  }
  
  void update_status()
  {
    int status;
    pid_t pid;
    do
      pid = waitpid(WAIT_ANY, &status, WUNTRACED | WNOHANG);
    while (!mark_process_status(pid, status));
  }
  
  void do_job_notification(int i)
  {
    update_status();
    for (auto job_it = dish_jobs.begin(); job_it < dish_jobs.end();)
    {
      auto &job = *job_it;
      if (job->is_completed())
      {
        if(job->background)
          fmt::println(job->format_job_info("completed"));
        job_it = dish_jobs.erase(job_it);
      }
      else if (job->is_stopped() && !job->notified)
      {
        if(job->background)
          fmt::println(job->format_job_info("stopped"));
        job->notified = true;
        ++job_it;
      }
      else
        ++job_it;
    }
    return;
  }
  
  [[noreturn]]void loop()
  {
    while (true)
    {
      std::string cmd;
      char hostname[256];
      gethostname(hostname, sizeof(hostname));
      auto uid = getuid();
      fmt::print("> Dish {}@{}:{} {} \n{} ",
                 utils::blue(getpwuid(uid)->pw_name),
                 utils::green(hostname),
                 utils::yellow(utils::simplify_path(utils::get_working_directory().value())),
                 dish_context.last_ret == 0 ? "" : utils::red("C: " + std::to_string(dish_context.last_ret)),
                 utils::red(uid == 0 ? "#" : "$"));
      std::getline(std::cin, cmd);
      if (!cmd.empty())
      {
        run(cmd);
      }
    }
  }
}
