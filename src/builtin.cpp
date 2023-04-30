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
#include "dish/utils.hpp"
#include "dish/dish.hpp"
#include "dish/builtin.hpp"

#include <unistd.h>

#include <vector>
#include <string>

extern char ** environ;

namespace dish::builtin
{
  int builtin_cd(Args args)
  {
    if (args.size() > 2)
    {
      fmt::println("cd: too many arguments.");
      return -1;
    }
    else if(args.size() < 1)
    {
      fmt::println("cd: too few arguments.");
      return -1;
    }
  
    auto last_dir_opt = utils::get_working_directory();
    std::string last_dir = "/";
    if(last_dir_opt.has_value())
      last_dir = last_dir_opt.value();
    
    if (args.size() == 1)
    {
      auto home_opt = utils::get_home();
      if (!home_opt.has_value())
      {
        fmt::println("cd: Can not find ~");
        return -1;
      }
      if (chdir(home_opt.value().c_str()) != 0)
      {
        fmt::println("cd: {}", strerror(errno));
        return -1;
      }
    }
    else if(args[1] == "-")
    {
      if(!dish_context.last_dir.empty())
      {
        if (chdir(dish_context.last_dir.c_str()) != 0)
        {
          fmt::println("cd: {}", strerror(errno));
          return -1;
        }
      }
      else
      {
        fmt::println("cd: Invalid '-'.");
        return -1;
      }
    }
    else if (chdir(args[1].c_str()) != 0)
    {
      fmt::println("cd: {}", strerror(errno));
      return -1;
    }
    dish_context.last_dir = last_dir;
    return 0;
  }
  int builtin_pwd(Args args)
  {
    if (args.size() != 1)
    {
      fmt::println("pwd: too many arguments.");
      return -1;
    }
    else
    {
      auto cwd = utils::get_working_directory();
      if(!cwd.has_value())
      {
        fmt::println("pwd: {}", strerror(errno));
        return -1;
      }
      fmt::println(cwd.value());
    }
    return 0;
  }
  
  int builtin_export(Args args)
  {
    if(args.size() == 1)
    {
      char ** envir = environ;
      while(*envir)
      {
        fmt::println("{}",*envir);
        envir++;
      }
    }
    else if(args.size() == 2)
    {
      auto eq = args[1].find('=');
      if(eq != std::string::npos)
      {
        auto name = args[1].substr(0, eq);
        auto value = args[1].substr(eq + 1);
        if(setenv(name.c_str(), value.c_str(), 1) == -1)
        {
          fmt::println("setenv: {}", strerror(errno));
          return -1;
        }
      }
    }
    return 0;
  }
  int builtin_jobs(Args)
  {
    for (size_t i = 0; i < dish_context.jobs.size(); ++i)
    {
      auto& job = dish_context.jobs[i];
      job->update_status();
      std::string job_info;
      if (job->is_completed())
        job_info = job->format_job_info("completed");
      else if (job->is_stopped())
        job_info = job->format_job_info("stopped");
      else
        job_info = job->format_job_info("running");
      fmt::println( job_info);
    }
    return 0;
  }
  int builtin_fg(Args args)
  {
    std::shared_ptr<job::Job> job = nullptr;
    if (args.size() > 2)
    {
      fmt::println("fg: too many arguments.");
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
        fmt::println("fg: invalid argument.");
        return -1;
      }
      if (id - 1 < 0 || id - 1 >= dish_context.jobs.size())
      {
        fmt::println("fg: invalid job id.");
        return -1;
      }
      job = dish_context.jobs[id - 1];
    }
    else if (args.size() == 1)
    {
      for (auto it = dish_context.jobs.crbegin(); it < dish_context.jobs.crend(); ++it)
      {
        if ((*it)->is_background() || (*it)->is_stopped())
          job = *it;
      }
      if (job == nullptr)
      {
        fmt::println("fg: no current job");
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
      fmt::println("bg: too many arguments.");
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
        fmt::println("bg: invalid argument.");
        return -1;
      }
      if (id - 1 < 0 || id - 1 >= dish_context.jobs.size())
      {
        fmt::println("bg: invalid job id.");
        return -1;
      }
      job = dish_context.jobs[id - 1];
    }
    else if (args.size() == 1)
    {
      for (auto it = dish_context.jobs.crbegin(); it < dish_context.jobs.crend(); ++it)
      {
        if ((*it)->is_background() || (*it)->is_stopped())
          job = *it;
      }
      if (job == nullptr)
      {
        fmt::println("bg: no current job");
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
      for(auto& r : dish_context.alias)
        fmt::println("{}={}", r.first, r.second);
    }
    else if(args.size() == 2)
    {
      auto eq = args[1].find('=');
      if(eq != std::string::npos)
      {
        auto name = args[1].substr(0, eq);
        auto alias = args[1].substr(eq + 1);
        dish_context.alias[name] = alias;
      }
      else
      {
        auto it = dish_context.alias.find(args[1]);
        if (it != dish_context.alias.end())
          fmt::println("{}={}", it->first, it->second);
      }
    }
    return 0;
  }

  int builtin_history(Args args)
  {
    int index = 1;
    for (auto &r : dish_context.history)
    {
      fmt::println("{}| {}", index, r);
      index++;
    }
    return 0;
  }
  
  int builtin_help(Args args)
  {
    std::vector<std::string> list;
    for (auto &r:builtins)
      list.emplace_back(utils::light_blue(r.first));
    
    fmt::println("{} \n {} \n {}",
        "Dish - caozhanhao, version 0.0.1-beta.",
        "These are builtin-commands. Use 'help' to see this.",
        fmt::join(list, ", ")
    );
    return 0;
  }
}
