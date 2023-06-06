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
#include "dish/line_editor.hpp"
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
    if (!dish_context.waiting)
      do_job_notification();
  }

  std::string dish_default_prompt()
  {
    return (getuid() == 0 ? "#" : "$");
  }

  void dish_init()
  {
    dish_context.running = true;
    dish_context.lua_state.open_libraries(sol::lib::base, sol::lib::string,
                                          sol::lib::coroutine, sol::lib::os,
                                          sol::lib::io, sol::lib::math,
                                          sol::lib::table, sol::lib::bit32);
    dish_context.lua_state.set_exception_handler(lua::dish_sol_exception_handler);
    dish_context.lua_state["dish"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["environment"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["alias"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["func"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["last_foreground_ret"] = sol::nil;
    dish_context.lua_state["dish"]["last_dir"] = utils::get_home();
    dish_context.lua_state["dish"]["history_path"] = "dish_history";
    dish_context.lua_state["dish"]["prompt"] = sol::nil;
    dish_context.lua_state["dish"]["hint"] = sol::nil;
    dish_context.lua_state["dish"]["complete"] = sol::nil;
    dish_context.lua_state["dish"]["style"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["style"]["cmd"] = static_cast<int>(utils::Effect::fg_blue);
    dish_context.lua_state["dish"]["style"]["arg"] = static_cast<int>(utils::Effect::fg_cyan);
    dish_context.lua_state["dish"]["style"]["string"] = static_cast<int>(utils::Effect::fg_yellow);
    dish_context.lua_state["dish"]["style"]["env"] = static_cast<int>(utils::Effect::fg_green);
    dish_context.lua_state["dish"]["style"]["error"] = static_cast<int>(utils::Effect::fg_red);
    //    clear = 0, bold, faint, italic, underline, slow_blink, rapid_blink, color_reverse,
    //    fg_black = 30, fg_red, fg_green, fg_yellow, fg_blue, fg_magenta, fg_cyan, fg_white,
    //    bg_black = 40, bg_red, bg_green, bg_yellow, bg_blue, bg_magenta, bg_cyan, bg_white
    dish_context.lua_state["dish"]["effects"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["effects"]["clear"] = 0;
    dish_context.lua_state["dish"]["effects"]["bold"] = 1;
    dish_context.lua_state["dish"]["effects"]["faint"] = 2;
    dish_context.lua_state["dish"]["effects"]["italic"] = 3;
    dish_context.lua_state["dish"]["effects"]["underline"] = 4;
    dish_context.lua_state["dish"]["effects"]["slow_blink"] = 5;
    dish_context.lua_state["dish"]["effects"]["rapid_blink"] = 6;
    dish_context.lua_state["dish"]["effects"]["color_reverse"] = 7;
    dish_context.lua_state["dish"]["effects"]["fg_black"] = 30;
    dish_context.lua_state["dish"]["effects"]["fg_red"] = 31;
    dish_context.lua_state["dish"]["effects"]["fg_green"] = 32;
    dish_context.lua_state["dish"]["effects"]["fg_yellow"] = 33;
    dish_context.lua_state["dish"]["effects"]["fg_blue"] = 34;
    dish_context.lua_state["dish"]["effects"]["fg_magenta"] = 34;
    dish_context.lua_state["dish"]["effects"]["fg_cyan"] = 36;
    dish_context.lua_state["dish"]["effects"]["fg_white"] = 37;
    dish_context.lua_state["dish"]["effects"]["bg_black"] = 40;
    dish_context.lua_state["dish"]["effects"]["bg_red"] = 41;
    dish_context.lua_state["dish"]["effects"]["bg_green"] = 42;
    dish_context.lua_state["dish"]["effects"]["bg_yellow"] = 43;
    dish_context.lua_state["dish"]["effects"]["bg_blue"] = 44;
    dish_context.lua_state["dish"]["effects"]["bg_magenta"] = 45;
    dish_context.lua_state["dish"]["effects"]["bg_cyan"] = 46;
    dish_context.lua_state["dish"]["effects"]["bg_white"] = 47;

    dish_context.lua_state["dish_add_history"]=
        [](std::string timestamp, std::string cmd)
    {
      line_editor::dle_context.history.emplace_back(line_editor::History{cmd, timestamp});
    };

    dish_context.waiting = false;
    dish_context.terminal = STDIN_FILENO;
    dish_context.is_interactive = isatty(dish_context.terminal);

    char **envir = environ;
    while (*envir)
    {
      std::string tmp{*envir};
      auto eq = tmp.find('=');
      if (eq == std::string::npos)
      {
        fmt::println(stderr, "Unexpected env: {}", tmp);
        std::exit(-1);
      }
      dish_context.lua_state["dish"]["environment"][tmp.substr(0, eq)] = tmp.substr(eq + 1);
      envir++;
    }

    dish_context.lua_state["dish"]["environment"]["PWD"] = std::filesystem::current_path().string();
    dish_context.lua_state["dish"]["environment"]["USERNAME"] = getpwuid(getuid())->pw_name;
    dish_context.lua_state["dish"]["environment"]["HOME"] = getpwuid(getuid())->pw_dir;
    dish_context.lua_state["dish"]["environment"]["UID"] = std::to_string(getuid());

    if (!dish_context.lua_state["dish"]["environment"]["PATH"].valid())
    {
      dish_context.lua_state["dish"]["environment"]["PATH"] = "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
    }

    dish_context.lua_state.script_file("../src/dishrc.lua");

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
        fmt::println(stderr, "DishError: Failed to put the shell in its own process group.");
        //std::exit(1);
      }
      tcsetpgrp(dish_context.terminal, dish_context.pgid);
      tcgetattr(dish_context.terminal, &dish_context.tmodes);
      // Put the terminal to raw mode for line editing.
      dish_context.tmodes.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
      dish_context.tmodes.c_cflag |= (CS8);
      dish_context.tmodes.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
      dish_context.tmodes.c_cc[VMIN] = 1;
      dish_context.tmodes.c_cc[VTIME] = 0;
      tcsetattr(dish_context.terminal, TCSAFLUSH, &dish_context.tmodes);
    }
  }
  
  void run_command(const std::string &cmd)
  {
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

  std::vector<std::string> get_path(bool with_curr)
  {
    std::vector<std::string> ret;
    if(auto p = dish_context.lua_state["dish"]["environment"]["PATH"]; p.valid())
      ret = utils::split<std::vector<std::string>>(p.get<std::string>(), ":");
    if(with_curr)
      ret.emplace_back(std::filesystem::current_path().string());
    return ret;
  }
}
