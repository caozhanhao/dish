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

#include "dish/job.hpp"
#include "dish/lexer.hpp"
#include "dish/line_editor.hpp"
#include "dish/parser.hpp"
#include "dish/utils.hpp"

#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

extern char **environ;

namespace dish
{
  dish::DishContext dish_context;

  auto to_token(const String &cmd)
  {
    return lexer::Lexer(cmd).get_all_tokens_no_check();
  }
  void sigchld_handler(int sig)
  {
    if (!dish_context.waiting)
      do_job_notification();
  }

  String dish_default_prompt()
  {
    return (getuid() == 0 ? "#" : "$");
  }

  // Dish initialize
  void dish_init()
  {
    dish_context.running = true;

    //
    // Terminal
    //
    dish_context.waiting = false;
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
      signal(SIGCHLD, sigchld_handler);

      dish_context.pgid = getpid();
      if (setpgid(dish_context.pgid, dish_context.pgid) < 0)
      {
        fmt::println(stderr, "DishError: Failed to put the shell in its own process group.");
        //std::exit(1);
      }
      tcsetpgrp(dish_context.terminal, dish_context.pgid);
      tcgetattr(dish_context.terminal, &dish_context.tmodes);
    }

    //
    // Lua
    //
    dish_context.lua_state.open_libraries(sol::lib::base, sol::lib::string,
                                          sol::lib::coroutine, sol::lib::os,
                                          sol::lib::io, sol::lib::math,
                                          sol::lib::table, sol::lib::bit32);
    dish_context.lua_state.set_exception_handler(lua::dish_sol_exception_handler);
    // basic table
    dish_context.lua_state["dish"] = dish_context.lua_state.create_table();
    // environment
    dish_context.lua_state["dish"]["environment"] = dish_context.lua_state.create_table();
    char **envir = environ;
    while (*envir)
    {
      String tmp{*envir};
      auto eq = tmp.find('=');
      if (eq == String::npos)
      {
        fmt::println(stderr, "Unexpected env: {}", tmp);
        std::exit(-1);
      }
      dish_context.lua_state["dish"]["environment"][tmp.substr(0, eq).cpp_str()] = tmp.substr(eq + 1).cpp_str();
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
    // alias
    dish_context.lua_state["dish"]["alias"] = dish_context.lua_state.create_table();
    // lua function
    dish_context.lua_state["dish"]["func"] = dish_context.lua_state.create_table();
    // ret
    dish_context.lua_state["dish"]["last_foreground_ret"] = sol::nil;
    // for cd -
    dish_context.lua_state["dish"]["last_dir"] = utils::get_home()->cpp_str();
    // history
    dish_context.lua_state["dish"]["history_path"] = utils::get_home()->cpp_str() + "/.local/share/dish/dish_history";
    dish_context.lua_state["dish_add_history"] =
            [](std::string timestamp, std::string cmd) {
              line_editor::dle_context.history.emplace_back(line_editor::History{cmd, timestamp});
            };
    // prompt
    dish_context.lua_state["dish"]["prompt"] = sol::nil;
    // prompt utils
    dish_context.lua_state["dish_get_tilde_path"] =
            []() { return utils::tilde(std::filesystem::current_path().string()).cpp_str(); };
    dish_context.lua_state["dish_get_shrunk_path"] =
            []() { return utils::shrink_path(utils::tilde(std::filesystem::current_path().string())).cpp_str(); };
    // complete, hint
    dish_context.lua_state["dish"]["enable_hint"] = true;
    dish_context.lua_state["dish"]["hint"] = sol::nil;
    dish_context.lua_state["dish"]["complete"] = sol::nil;
    // Dish Line Editor style
    dish_context.lua_state["dish"]["style"] = dish_context.lua_state.create_table();
    dish_context.lua_state["dish"]["style"]["cmd"] = static_cast<int>(utils::Effect::fg_blue);
    dish_context.lua_state["dish"]["style"]["arg"] = static_cast<int>(utils::Effect::fg_cyan);
    dish_context.lua_state["dish"]["style"]["string"] = static_cast<int>(utils::Effect::fg_yellow);
    dish_context.lua_state["dish"]["style"]["env"] = static_cast<int>(utils::Effect::fg_green);
    dish_context.lua_state["dish"]["style"]["error"] = static_cast<int>(utils::Effect::fg_red);
    dish_context.lua_state["dish"]["style"]["info"] = static_cast<int>(utils::Effect::fg_magenta);
    // effects for style
    // bold = 1, faint, italic, underline, slow_blink, rapid_blink, color_reverse,
    // fg_black = 30, fg_red, fg_green, fg_yellow, fg_blue, fg_magenta, fg_cyan, fg_white,
    // bg_black = 40, bg_red, bg_green, bg_yellow, bg_blue, bg_magenta, bg_cyan, bg_white
    dish_context.lua_state["dish"]["effects"] = dish_context.lua_state.create_table();
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

    // load config
    dish_context.lua_state.script_file(utils::get_home().value().cpp_str() + "/.config/dish/config.lua");
  }

  void run_command(const String &cmd)
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
        if (job->is_background())
          fmt::println("\n{}", job->format_job_info("completed"));
        job_it = dish_context.jobs.erase(job_it);
      }
      else if (job->is_stopped() && !job->notified)
      {
        if (job->is_background())
          fmt::println("\n{}", job->format_job_info("stopped"));
        job->notified = true;
        ++job_it;
      }
      else
        ++job_it;
    }
    return;
  }

  std::vector<String> get_path(bool with_curr)
  {
    std::vector<String> ret;
    if (auto p = dish_context.lua_state["dish"]["environment"]["PATH"]; p.valid())
      ret = utils::split<std::string_view, std::vector<String>>(p.get<std::string>(), ":");
    if (with_curr)
      ret.emplace_back(std::filesystem::current_path().string());
    return ret;
  }
}// namespace dish
