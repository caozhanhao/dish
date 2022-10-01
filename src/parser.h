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
#ifndef DISH_PARSER_H
#define DISH_PARSER_H

#include "error.h"
#include "lexer.h"
#include "command.h"
#include <string>
#include <vector>

namespace dish::parser
{
  class Parser
  {
  private:
    cmd::Command command;
    std::shared_ptr<cmd::SingleCmd> scmd;
    DishInfo *info;
    std::vector<lexer::Token> tokens;
    std::size_t pos;
  public:
    Parser(DishInfo *info_, std::vector<lexer::Token> tokens);
    
    void parse();
    
    cmd::Command get_cmd() const;
  
  private:
    void get_tokens();
    
    void parse_if();
    
    void parse_cmd(cmd::Command &);
  };
}
#endif