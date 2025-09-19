//   Copyright 2022 - 2025 dish - caozhanhao
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

#include "dish/builtin.hpp"
#include "dish/args_parser.hpp"
#include "dish/dish.hpp"
#include "dish/dish_lua.hpp"
#include "dish/job.hpp"
#include "dish/line_editor.hpp"
#include "dish/utils.hpp"

#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <list>
#include <string>
#include <vector>

namespace dish::builtin
{
  int builtin_cd(Args args)
  {
    if (args.size() > 2)
    {
      fmt::println(stderr, "cd: too many arguments.");
      return -1;
    }
    else if (args.size() < 1)
    {
      fmt::println(stderr, "cd: too few arguments.");
      return -1;
    }

    String last_dir = std::filesystem::current_path().string();

    if (args.size() == 1)
    {
      auto home_opt = utils::get_home();
      if (!home_opt.has_value())
      {
        fmt::println(stderr, "cd: Can not find ~");
        return -1;
      }
      if (chdir(home_opt.value().c_str()) != 0)
      {
        fmt::println(stderr, "cd: {}", strerror(errno));
        return -1;
      }
      dish_context.lua_state["dish"]["environment"]["PWD"] = home_opt.value().cpp_str();
    }
    else if (args[1] == "-")
    {
      if (dish_context.lua_state["dish"]["last_dir"].valid())
      {
        if (chdir(dish_context.lua_state["dish"]["last_dir"].get<const char *>()) != 0)
        {
          fmt::println(stderr, "cd: {}", strerror(errno));
          return -1;
        }
        dish_context.lua_state["dish"]["environment"]["PWD"] = dish_context.lua_state["dish"]["last_dir"];
      }
      else
      {
        fmt::println(stderr, "cd: Invalid '-'.");
        return -1;
      }
    }
    else
    {
      if (chdir(args[1].c_str()) != 0)
      {
        fmt::println(stderr, "cd: {}", strerror(errno));
        return -1;
      }
      dish_context.lua_state["dish"]["environment"]["PWD"] = std::filesystem::current_path().string();
    }
    dish_context.lua_state["dish"]["last_dir"] = last_dir.cpp_str();
    return 0;
  }
  int builtin_pwd(Args args)
  {
    if (args.size() != 1)
    {
      fmt::println(stderr, "pwd: too many arguments.");
      return -1;
    }
    else
    {
      if (auto s = dish_context.lua_state["dish"]["environment"]["PWD"]; s.valid())
        fmt::println(s.get<std::string>());
      else
      {
        auto path = std::filesystem::current_path().string();
        dish_context.lua_state["dish"]["environment"]["PWD"] = path;
        fmt::println(path);
      }
    }
    return 0;
  }

  int builtin_export(Args args)
  {
    if (args.size() == 1)
    {
      for (auto &r: dish_context.lua_state["dish"]["environment"].get<sol::table>())
        fmt::println("{}={}", r.first.as<std::string>(), r.second.as<std::string>());
    }
    else if (args.size() == 2)
    {
      auto eq = args[1].find('=');
      if (eq != String::npos)
      {
        auto name = args[1].substr(0, eq);
        auto value = args[1].substr(eq + 1);
        dish_context.lua_state["dish"]["environment"][name.cpp_str()] = value.cpp_str();
      }
      else
        dish_context.lua_state["dish"]["environment"][args[1].cpp_str()] = "";
    }
    return 0;
  }

  int builtin_unset(Args args)
  {
    if (args.size() < 2)
    {
      fmt::println(stderr, "unset: too few arguments.");
      return -1;
    }
    else if (args.size() > 2)
    {
      fmt::println(stderr, "unset: too many arguments.");
      return -1;
    }
    else
    {
      auto it = dish_context.lua_state["dish"]["environment"][args[1].cpp_str()];
      if (!it.valid())
      {
        fmt::println(stderr, "unset: Unknown name.");
        return -1;
      }
      it = sol::nil;
    }
    return 0;
  }

  int builtin_jobs(Args)
  {
    for (size_t i = 0; i < dish_context.jobs.size(); ++i)
    {
      auto &job = utils::list_at(dish_context.jobs, i);
      job->update_status();
      String job_info;
      if (job->is_completed())
        job_info = job->format_job_info("completed");
      else if (job->is_stopped())
        job_info = job->format_job_info("stopped");
      else
        job_info = job->format_job_info("running");
      fmt::println(job_info.cpp_str());
    }
    return 0;
  }
  int builtin_fg(Args args)
  {
    std::shared_ptr<job::Job> job = nullptr;
    if (args.size() > 2)
    {
      fmt::println(stderr, "fg: too many arguments.");
      return -1;
    }
    else if (args.size() == 2)
    {
      int id;
      try
      {
        id = std::stoi(args[1].cpp_str());
      } catch (std::invalid_argument &)
      {
        fmt::println(stderr, "fg: invalid argument.");
        return -1;
      }
      if (id - 1 < 0 || id - 1 >= dish_context.jobs.size())
      {
        fmt::println(stderr, "fg: invalid job id.");
        return -1;
      }
      job = utils::list_at(dish_context.jobs, id - 1);
    }
    else if (args.size() == 1)
    {
      for (auto it = dish_context.jobs.crbegin(); it != dish_context.jobs.crend(); ++it)
      {
        if ((*it)->is_background() || (*it)->is_stopped())
          job = *it;
      }
      if (job == nullptr)
      {
        fmt::println(stderr, "fg: no current job");
        return -1;
      }
    }
    fmt::println(job->format_job_info("running").cpp_str());
    job->set_foreground();
    if (job->is_stopped())
      job->continue_job();
    else
      job->put_in_foreground(0);
    return 0;
  }

  int builtin_bg(Args args)
  {
    std::shared_ptr<job::Job> job = nullptr;
    if (args.size() > 2)
    {
      fmt::println(stderr, "bg: too many arguments.");
      return -1;
    }
    else if (args.size() == 2)
    {
      int id;
      try
      {
        id = std::stoi(args[1].cpp_str());
      } catch (std::invalid_argument &)
      {
        fmt::println(stderr, "bg: invalid argument.");
        return -1;
      }
      if (id - 1 < 0 || id - 1 >= dish_context.jobs.size())
      {
        fmt::println(stderr, "bg: invalid job id.");
        return -1;
      }
      job = utils::list_at(dish_context.jobs, id - 1);
    }
    else if (args.size() == 1)
    {
      for (auto it = dish_context.jobs.crbegin(); it != dish_context.jobs.crend(); ++it)
      {
        if ((*it)->is_background() || (*it)->is_stopped())
          job = *it;
      }
      if (job == nullptr)
      {
        fmt::println(stderr, "bg: no current job");
        return -1;
      }
    }
    fmt::println(job->format_job_info("running").cpp_str());
    job->set_background();
    if (job->is_stopped())
      job->continue_job();
    else
      job->put_in_background(0);
    return 0;
  }

  int builtin_exit(Args)
  {
    dish_context.running = false;
    return 0;
  }

  int builtin_alias(Args args)
  {
    if (args.size() == 1)
    {
      for (auto &r: dish_context.lua_state["dish"]["alias"].get<sol::table>())
        fmt::println("{}={}", r.first.as<String>(), r.second.as<String>());
    }
    else if (args.size() == 2)
    {
      auto eq = args[1].find('=');
      if (eq != String::npos)
      {
        auto name = args[1].substr(0, eq);
        auto alias = args[1].substr(eq + 1);
        dish_context.lua_state["dish"]["alias"][name.cpp_str()] = alias.cpp_str();
      }
      else
      {
        auto it = dish_context.lua_state["dish"]["alias"][args[1].cpp_str()];
        if (it.valid())
          fmt::println("{}={}", args[1], it.get<std::string>());
      }
    }
    return 0;
  }

  int builtin_history(Args args)
  {
    int i = 1;
    for (auto &r: line_editor::dle_context.history)
    {
      fmt::println("{}| {}", i++, r.cmd);
    }
    return 0;
  }

  int builtin_help(Args args)
  {
    std::vector<std::string> list;
    for (auto &r: builtins)
      list.emplace_back(utils::cyan(r.first).cpp_str());

    fmt::println("{} \n {} \n {}",
                 "Dish - caozhanhao",
                 "These are builtin-commands. Use 'help' to see this.",
                 fmt::join(list, ", "));
    return 0;
  }

  int builtin_type(Args args)
  {
    if (args.size() == 1)
    {
      fmt::println(stderr, "type: too few arguments.");
      return -1;
    }
    for (auto it = args.cbegin() + 1; it != args.cend(); ++it)
    {
      if (auto alias = dish_context.lua_state["dish"]["alias"][it->cpp_str()]; alias.valid())
        fmt::println("{} is an alias for {}", *it, alias.get<std::string>());
      else
      {
        auto [type, cmd] = utils::find_command(*it);
        switch (type)
        {
          case utils::CommandType::not_found:
          case utils::CommandType::not_executable:
            fmt::println("{} not found", *it);
            break;
          case utils::CommandType::builtin:
            fmt::println("{} is a shell builtin", *it);
            break;
          case utils::CommandType::lua_func:
            fmt::println("{} is a lua function.", *it);
            break;
          case utils::CommandType::executable_file:
          case utils::CommandType::executable_link:
            fmt::println("{} is {}", *it, cmd);
            break;
        }
      }
    }
    return 0;
  }
  int builtin_source(Args args)
  {
    args::ArgsParser args_parser;
    args_parser.add_description("Run a dish script (currently lua).");
    args_parser.add_option<String>("__self", "")
            .add_restrictor(args::existing_path());
    args_parser.add_option<String>("path", "p")
            .add_restrictor(args::existing_path())
            .add_description("Run from path.");
    args_parser.add_option<String>("str", "s")
            .add_description("Run from string.");
    args_parser.add_help("help", "h")
            .add_description("Show this help.");
    args_parser.parse(args);

    if (auto p = args_parser.get<String>("path"); p.has_value())
      dish_context.lua_state.script_file(p.value().cpp_str(), &lua::dish_sol_error_handler);
    else if (auto sp = args_parser.get<String>("__self"); sp.has_value())
      dish_context.lua_state.script_file(sp.value().cpp_str(), &lua::dish_sol_error_handler);
    else if (auto s = args_parser.get<String>("str"); s.has_value())
      dish_context.lua_state.script(s.value().cpp_str(), &lua::dish_sol_error_handler);
    return 0;
  }
}// namespace dish::builtin
