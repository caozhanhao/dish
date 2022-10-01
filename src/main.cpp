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
#include "dish.h"
#include "option.h"

using namespace dish;

int main(int argc, char **argv)
{
  Dish dish;
  option::Option option(argc, argv);
  option.add("dish", [&dish](option::Option::CallbackArgType args)
  {
    for (auto &cmd: args)
    {
      dish.run(cmd);
    }
  });
  option.parse().run();
  dish.loop();
  return 0;
}