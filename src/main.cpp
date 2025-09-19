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
#include "dish/type_alias.hpp"
#include "dish/utils.hpp"

using namespace dish;

int main(int argc, char **argv)
{
  dish_init();
  String history = dish_context.lua_state["dish"]["history_path"].get<std::string>();
  line_editor::dle_init();
  line_editor::load_history(history);
  while (dish_context.running)
  {
    String prompt;
    if (auto h = dish_context.lua_state["dish"]["prompt"]; h.valid())
    {
      auto res = h();
      if (res.valid() && res.get_type() == sol::type::string)
        prompt = res.get<std::string>();
    }
    if (prompt.empty()) prompt = dish_default_prompt();

    String line = line_editor::read_line(prompt);
    run_command(line);
  }
  line_editor::save_history(history);
  return 0;
}