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

#include "dish/dish.hpp"
#include "dish/line_editor.hpp"
#include "dish/utils.hpp"

int main(int argc, char **argv)
{
  dish::dish_init();
  std::string history = dish::dish_context.lua_state["dish"]["history_path"];
  dish::line_editor::dle_init();
  dish::line_editor::load_history(history);
  while (dish::dish_context.running)
  {
    auto prompt_lua = dish::dish_context.lua_state["dish"]["prompt"]();
    std::string prompt;
    if(!prompt_lua.valid())
    {
      sol::error err = prompt_lua;
      fmt::println(stderr, "Unexpected result from dish.prompt:\n{}", err.what());
      prompt = dish::dish_default_prompt();
    }
    else
      prompt = prompt_lua.get<std::string>().c_str();
    std::string line = dish::line_editor::read_line(prompt);
    dish::run_command(line);
  }
  dish::line_editor::save_history(history);
  return 0;
}