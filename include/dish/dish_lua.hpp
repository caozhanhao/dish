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
#ifndef DISH_DISH_LUA_HPP
#define DISH_DISH_LUA_HPP
#pragma once

#include "bundled/sol/sol.hpp"

namespace dish::lua
{
  sol::protected_function_result dish_sol_error_handler(lua_State *L, sol::protected_function_result pfr);
  int dish_sol_exception_handler(lua_State *L, sol::optional<const std::exception &> maybe_exception, sol::string_view description);
}// namespace dish::lua
#endif