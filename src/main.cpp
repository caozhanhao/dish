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
    std::string prompt;
    if (sol::protected_function h = dish::dish_context.lua_state["dish"]["prompt"]; h.valid())
    {
      auto res = h();
      if (res.valid() && res.get_type() == sol::type::string)
        prompt = res.get<std::string>();
    }
    if(prompt.empty()) prompt = dish::dish_default_prompt();

    std::string line = dish::line_editor::read_line(prompt);
    dish::run_command(line);
  }
  dish::line_editor::save_history(history);
  return 0;
}