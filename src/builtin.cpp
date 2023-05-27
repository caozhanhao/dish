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

#include "dish/builtin.hpp"
#include "dish/argsparser.hpp"
#include "dish/dish.hpp"
#include "dish/job.hpp"
#include "dish/utils.hpp"
#include "dish/dish_script.hpp"

#include <unistd.h>

#include <vector>
#include <list>
#include <algorithm>
#include <filesystem>
#include <string>

namespace dish::builtin
{
  int builtin_cd(Args args)
  {
    if (args.size() > 2)
    {
      fmt::println(stderr, "cd: too many arguments.");
      return -1;
    }
    else if(args.size() < 1)
    {
      fmt::println(stderr, "cd: too few arguments.");
      return -1;
    }
  
    std::string last_dir = std::filesystem::current_path().string();

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
    }
    else if(args[1] == "-")
    {
      if(dish_context.lua_state["dish"]["last_dir"].valid())
      {
        if (chdir(dish_context.lua_state["dish"]["last_dir"].get<const char*>()) != 0)
        {
          fmt::println(stderr, "cd: {}", strerror(errno));
          return -1;
        }
        dish_context.lua_state["dish"]["environment"]["PWD"]
        = dish_context.lua_state["dish"]["last_dir"];
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
    dish_context.lua_state["dish"]["last_dir"] = last_dir;
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
      if(auto s = dish_context.lua_state["dish"]["environment"]["PWD"]; s.valid())
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
    if(args.size() == 1)
    {
      for (auto &r: dish_context.lua_state["dish"]["environment"].get<sol::table>())
        fmt::println("{}={}", r.first.as<std::string>(), r.second.as<std::string>());
    }
    else if(args.size() == 2)
    {
      auto eq = args[1].find('=');
      if(eq != std::string::npos)
      {
        auto name = args[1].substr(0, eq);
        auto value = args[1].substr(eq + 1);
        dish_context.lua_state["dish"]["environment"][name] = value;
      }
      else
        dish_context.lua_state["dish"]["environment"][args[1]] = "";
    }
    return 0;
  }

  int builtin_unset(Args args)
  {
    if(args.size() < 2)
    {
      fmt::println(stderr, "unset: too few arguments.");
      return -1;
    }
    else if(args.size() > 2)
    {
      fmt::println(stderr, "unset: too many arguments.");
      return -1;
    }
    else
    {
      auto it = dish_context.lua_state["dish"]["environment"][args[1]];
      if(!it.valid())
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
      auto& job = utils::list_at(dish_context.jobs, i);
      job->update_status();
      std::string job_info;
      if (job->is_completed())
        job_info = job->format_job_info("completed");
      else if (job->is_stopped())
        job_info = job->format_job_info("stopped");
      else
        job_info = job->format_job_info("running");
      fmt::println(job_info);
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
        id = std::stoi(args[1]);
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
    fmt::println(job->format_job_info("running"));
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
        id = std::stoi(args[1]);
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
    fmt::println(job->format_job_info("running"));
    job->set_background();
    if (job->is_stopped())
      job->continue_job();
    else
      job->put_in_background(0);
    return 0;
  }
  
  int builtin_exit(Args)
  {
    std::exit(0);
    return 0;
  }

  int builtin_alias(Args args)
  {
    if(args.size() == 1)
    {
      for(auto& r : dish_context.lua_state["dish"]["alias"].get<sol::table>())
        fmt::println("{}={}", r.first.as<std::string>(), r.second.as<std::string>());
    }
    else if(args.size() == 2)
    {
      auto eq = args[1].find('=');
      if(eq != std::string::npos)
      {
        auto name = args[1].substr(0, eq);
        auto alias = args[1].substr(eq + 1);
        dish_context.lua_state["dish"]["alias"][name] = alias;
      }
      else
      {
        auto it = dish_context.lua_state["dish"]["alias"][args[1]];
        if (it.valid())
          fmt::println("{}={}", args[1], it.get<std::string>());
      }
    }
    return 0;
  }

  int builtin_history(Args args)
  {
    for (auto &r : dish_context.lua_state["dish"]["history"].get<sol::table>())
    {
      fmt::println("{}| {}", r.first.as<int>(), r.second.as<std::string>());
    }
    return 0;
  }
  
  int builtin_help(Args args)
  {
    std::vector<std::string> list;
    for (auto &r:builtins)
      list.emplace_back(utils::light_blue(r.first));
    
    fmt::println("{} \n {} \n {}",
        "Dish - caozhanhao",
        "These are builtin-commands. Use 'help' to see this.",
        fmt::join(list, ", ")
    );
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
      if (auto alias = dish_context.lua_state["dish"]["alias"][*it]; alias.valid())
        fmt::println("{} is an alias for {}", *it, alias.get<std::string>());
      else if(builtins.find(*it) != builtins.end())
        fmt::println("{} is a shell builtin", *it);
      else if(dish_context.lua_state["dish"]["func"][*it].valid())
        fmt::println("{} is a lua function", *it);
      else
      {
        bool found = false;
        std::string path;
        for (auto p : get_path(false))
        {
          path = p + "/" + *it;
          if (std::filesystem::exists(path))
          {
            found = true;
            break;
          }
        }
        if(!found)
          fmt::println("{} not found", *it);
        else
          fmt::println("{} is {}", *it, path);
      }
    }
    return 0;
  }
  int builtin_source(Args args)
  {
    args::ArgsParser args_parser;
    args_parser.add_description("Run a dish script (currently lua).");
    args_parser.add_option<std::string>("__self", "")
            .add_restrictor(args::existing_path());
    args_parser.add_option<std::string>("path", "p")
            .add_restrictor(args::existing_path())
            .add_description("Run from path.");
    args_parser.add_option<std::string>("str", "s")
            .add_description("Run from string.");
    args_parser.add_help("help", "h")
            .add_description("Show this help.");
    args_parser.parse(args);

    if (auto p = args_parser.get<std::string>("path"); p.has_value())
      dish_context.lua_state.script_file(p.value(), &script::dish_sol_error_handler);
    else if(auto sp = args_parser.get<std::string>("__self"); sp.has_value())
      dish_context.lua_state.script_file(sp.value(), &script::dish_sol_error_handler);
    else if (auto s = args_parser.get<std::string>("str"); s.has_value())
      dish_context.lua_state.script(s.value(), &script::dish_sol_error_handler);
    return 0;
  }
}
