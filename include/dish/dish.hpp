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
#ifndef DISH_DISH_HPP
#define DISH_DISH_HPP
#pragma once

#include "dish_lua.hpp"
#include "type_alias.hpp"
#include "utils.hpp"

#include <termios.h>

#include <atomic>
#include <list>
#include <memory>
#include <vector>

namespace dish
{
  namespace job
  {
    class Job;
  }

  struct DishContext
  {
    bool running;
    sol::state lua_state;

    std::list<std::shared_ptr<job::Job>> jobs;

    pid_t pgid;
    struct termios tmodes;
    int terminal;
    int is_interactive;

    std::atomic<bool> waiting;
  };

  extern DishContext dish_context;

  void dish_init();

  void run_command(const String &cmd);

  std::list<String> get_history();

  void do_job_notification();

  std::vector<String> get_path(bool with_curr = true);
  String dish_default_prompt();
}// namespace dish
#endif