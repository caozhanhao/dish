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

#include "dish/error.hpp"
#include "dish/utils.hpp"

#include <stdexcept>
#include <string>

namespace dish::error
{
  Error::Error(const std::string &errtype_, const std::string &msg_, const std::string &detail_)
      : runtime_error(utils::colorify(errtype_, utils::Color::RED) + ": \n" + msg_ + ": \n" + detail_ + "\n"),
        errtype(errtype_), msg(msg_), detail(detail_) {}
  
  [[nodiscard]] std::string Error::get_content() const
  {
    return {utils::colorify(errtype, utils::Color::RED) + ": \n" + msg + ": \n" + detail + "\n"};
  }
  
  DishError::DishError(std::string location_, const std::string &func_name_, const std::string &detail_)
      : logic_error(utils::colorify("Dish Error:", utils::Color::RED)
                    + location_ + ": \n\033[m" + detail_ + "\n"),
        location(std::move(location_) + ": " + func_name_ + "()"),
        detail(detail_) {}
  
  [[nodiscard]] std::string DishError::get_detail() const
  {
    return detail;
  }
  
  [[nodiscard]] std::string DishError::get_content() const
  {
    return {utils::colorify("Dish Error: ", utils::Color::RED)
            + location + ": \n\033[m" + detail + "\n"};
  }
  
}
