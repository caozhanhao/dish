//   Copyright 2022 dish - caozhanhao
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
#ifndef DISH_COMMAND_H
#define DISH_COMMAND_H

#include "error.h"
#include "builtin.h"
#include <string>
#include <variant>
#include <memory>

namespace dish { class Dish; }
namespace dish::cmd
{
  enum class RedirectType
  {
    output, input, appending
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
    
    int get(int mode) const;
  };
  
  class Command;
  
  class SingleCmd
  {
  protected:
    DishInfo *info;
  public:
    virtual int execute() = 0;
    
    void set_info(DishInfo *dish_);
  };
  
  class SimpleCmd : public SingleCmd
  {
  private:
    std::vector<std::string> args;
  public:
    SimpleCmd() = default;
    
    int execute() override;
    
    void insert(std::string str);
    
    void clear();
    
    bool empty() const;
  
  private:
    void expand_wildcards();
    
    const std::vector<std::string> &get_args() const;
    
    std::vector<char *> get_cargs() const;
  };
  
  class ForCmd : public SingleCmd
  {
  
  };
  
  class CaseCmd : public SingleCmd
  {
  
  };
  
  class WhileCmd : public SingleCmd
  {
  private:
    std::shared_ptr<Command> condition;
    std::shared_ptr<Command> action;
  public:
    int execute() override;
  };
  
  class IfCmd : public SingleCmd
  {
  private:
    std::shared_ptr<Command> condition;
    std::shared_ptr<Command> true_case;
    std::shared_ptr<Command> false_case;
  public:
    IfCmd(std::shared_ptr<Command> cond, std::shared_ptr<Command> true_c, std::shared_ptr<Command> false_c)
        : condition(cond), true_case(true_c), false_case(false_c) {}
    
    int execute() override;
  };
  
  class Connection : public SingleCmd
  {
  
  };
  
  class FuncDef : public SingleCmd
  {
  
  };
  
  class GroupDef : public SingleCmd
  {
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