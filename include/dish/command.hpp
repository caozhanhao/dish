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
#ifndef DISH_COMMAND_HPP
#define DISH_COMMAND_HPP
#pragma once

#include "error.hpp"
#include "builtin.hpp"

#include <string>
#include <list>
#include <vector>
#include <variant>
#include <memory>

namespace dish { class Dish; }
namespace dish::cmd
{
  enum class RedirectType
  {
    overwrite, append, input, fd
  };
  
  class Redirect
  {
  private:
    RedirectType type;
    std::variant<int, std::string> redirect;
  public:
    template<typename T, typename = std::enable_if_t<!std::is_base_of_v<Redirect, std::decay_t<T>>>>
    Redirect(RedirectType type_, T &&redirect_) : type(type_), redirect(redirect_) {}
    
    bool is_description() const;
    
    std::string get_filename() const;
    
    int get_description() const;
  
    int get() const;
  };
  
  class Command;
  
  class SingleCmd
  {
  protected:
    DishInfo *info;
  public:
    virtual std::pair<int, int> execute() = 0;
    
    void set_info(DishInfo *dish_);
  };
  
  class SimpleCmd : public SingleCmd
  {
  private:
    std::vector<std::string> args;
  public:
    SimpleCmd() = default;
  
    std::pair<int, int> execute() override;
    
    void insert(std::string str);
    
    void clear();
    
    bool empty() const;
  
  private:
    const std::vector<std::string> &get_args() const;
    
    std::vector<char *> get_cargs() const;
  };
  
  class Command
  {
  private:
    std::vector<std::shared_ptr<SingleCmd>> commands;
    Redirect out;
    Redirect in;
    Redirect err;
    bool background;
    DishInfo *info;
  public:
    Command(DishInfo *info_);
    
    int execute();
    
    void insert(std::shared_ptr<SingleCmd> scmd);
    
    void set_in(Redirect redirect);
    
    void set_out(Redirect redirect);
    
    void set_err(Redirect redirect);
    
    void set_info(DishInfo *dish_);
    
    void set_background();
  };
  
}
#endif