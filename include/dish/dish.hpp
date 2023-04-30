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
#ifndef DISH_DISH_HPP
#define DISH_DISH_HPP
#pragma once

#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <string>
#include <vector>
#include <list>
#include <atomic>
#include <memory>
#include <map>

namespace dish
{
  namespace job{class Job;}

  struct DishContext
  {
    int last_foreground_ret;
    std::string last_dir;
    std::map<std::string, std::string> alias;
    std::map<std::string,std::string> env;
    std::list<std::string> history;
    std::list<std::shared_ptr<job::Job>> jobs;

    pid_t pgid;
    struct termios tmodes;
    int terminal;
    int is_interactive;

    std::atomic<bool> waiting;
  };

  extern DishContext dish_context;

  extern void init();
  
  extern void run(const std::string &cmd);
  
  [[noreturn]] extern void loop();
  
  extern std::list<std::string> get_history();
  
  extern void do_job_notification();

  extern std::vector<std::string> get_path();
}
#endif