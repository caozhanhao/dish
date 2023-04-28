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
#ifndef DISH_ERROR_HPP
#define DISH_ERROR_HPP
#pragma once

#include <stdexcept>
#include <string>

#define DISH_STRINGFY(x) _DISH_STRINGFY(x)
#define _DISH_STRINGFY(x) #x
#define DISH_ERROR_LOCATION  __FILE__ ":" DISH_STRINGFY(__LINE__)
namespace dish::error
{
  class Error : public std::runtime_error
  {
  private:
    std::string errtype;
    std::string msg;
    std::string detail;
  public:
    Error(const std::string &errtype_, const std::string &msg_, const std::string &detail_);
    
    [[nodiscard]] std::string get_content() const;
  };
  
  class DishError : public std::logic_error
  {
  private:
    std::string location;
    std::string detail;
  public:
    DishError(std::string location_, const std::string &func_name_, const std::string &detail_);
    
    [[nodiscard]] std::string get_detail() const;
    
    [[nodiscard]] std::string get_content() const;
  };
}
#endif