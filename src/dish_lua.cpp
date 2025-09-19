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

#include "dish/dish_lua.hpp"
#include "dish/dish.hpp"
#include "dish/utils.hpp"

namespace dish::lua
{
  sol::protected_function_result dish_sol_error_handler(lua_State *L, sol::protected_function_result pfr)
  {
    std::exception_ptr eptr = std::current_exception();
    String errmsg;
    if (eptr) {
      try {
        std::rethrow_exception(eptr);
      } catch (const std::exception &ex) {
        errmsg += "std::exception -- ";
        errmsg.append(ex.what());
      } catch (const String &message) {
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
      errmsg.append(String(serr.data(), serr.size()));
    }

    fmt::println(stderr, "Dish Lua Error:\n{} error:\n{}",
                 sol::to_string(pfr.status()), errmsg);
    return script_pass_on_error(L, std::move(pfr));
  }
  int dish_sol_exception_handler(lua_State *L, sol::optional<const std::exception &> maybe_exception, sol::string_view description)
  {
    fmt::println(stderr, "An exception occurred in a function ");
    if (maybe_exception)
    {
      const std::exception &ex = *maybe_exception;
      fmt::println(stderr, "(straight from the exception):\n{}", ex.what());
    }
    else
    {
      fmt::println(stderr, "(from the description parameter):");
      std::cerr.write(description.data(), static_cast<std::streamsize>(description.size()));
      fmt::print("\n");
    }
    return sol::stack::push(L, description);
  }
}// namespace dish::lua
