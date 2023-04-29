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

#include "value.hpp"

#include <string>
#include <map>
#include <vector>

namespace dish
{
  class Dish;
  
  struct DishInfo
  {
    Dish *dish;
    std::map<std::string, value::Value> var_table;
    bool background;
    int last_ret;
    
    DishInfo(Dish *d);
  };
  
  class Dish
  {
  private:
    std::vector<std::string> history;
    DishInfo info;
  public:
    Dish();
    
    void run(const std::string &cmd);
  
    [[noreturn]] void loop();
    
    std::vector<std::string> get_history() const;
  };
}
#endif