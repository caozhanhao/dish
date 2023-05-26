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

#include "dish/utils.hpp"
#include "dish/dish_script.hpp"

namespace dish::script
{
  sol::protected_function_result dish_sol_error_handler(lua_State* L, sol::protected_function_result pfr)
  {
      std::exception_ptr eptr = std::current_exception();
      std::string errmsg;
      if (eptr) {
        try {
          std::rethrow_exception(eptr);
        } catch (const std::exception &ex) {
          errmsg += "std::exception -- ";
          errmsg.append(ex.what());
        } catch (const std::string &message) {
          errmsg += "message -- ";
          errmsg.append(message);
        } catch (const char *message) {
          errmsg += "message -- ";
          errmsg.append(message);
        } catch (...) {
          errmsg.append("unknown type, cannot serialize into error message");
        }
      }
      if (sol::type_of(L, pfr.stack_index()) == sol::type::string)
      {
        std::string_view serr = sol::stack::unqualified_get<std::string_view>(L, pfr.stack_index());
        errmsg.append(serr.data(), serr.size());
      }

      fmt::println(stderr, "Dish Script (currently Lua) Error:\n{} error:\n{}",
                   sol::to_string(pfr.status()), errmsg);
      return script_pass_on_error(L, std::move(pfr));
  }
}
