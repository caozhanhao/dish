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

#include "dish/lexer.hpp"
#include "dish/command.hpp"
#include "dish/parser.hpp"

#include <string>
#include <vector>

namespace dish::parser
{
  
  Parser::Parser(DishInfo *info_, std::vector<lexer::Token> tokens_)
      : info(info_), scmd(nullptr), command(info_), tokens(tokens_), pos(0) {}
  
  void Parser::parse()
  {
    pos = 0;
    while (pos < tokens.size())
    {
      parse_cmd(command);
    }
  }
  
  cmd::Command Parser::get_cmd() const { return command; }
  
  void Parser::parse_cmd(cmd::Command &cmd)
  {
    auto add_scmd = [&cmd, this]()
    {
      cmd::SimpleCmd scmd;
      while (pos < tokens.size() && tokens[pos].get_type() == lexer::TokenType::word)
      {
        scmd.insert(tokens[pos++].get_content());
      }
      cmd.insert(std::make_shared<cmd::SimpleCmd>(scmd));
    };
    add_scmd();
    while (pos < tokens.size() && tokens[pos].get_type() == lexer::TokenType::pipe)
    {
      ++pos;
      add_scmd();
    }
  
    while (pos < tokens.size())
    {
      //    word, lt, rt, newline, pipe,
      //    rt_and, rt_rt, rt_rt_and, background,
      //    end
      switch (tokens[pos].get_type())
      {
        case lexer::TokenType::rt:
          cmd.set_out(cmd::Redirect{cmd::RedirectType::output, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::rt_and:
          cmd.set_out(cmd::Redirect{cmd::RedirectType::appending, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::background:
          cmd.set_background();
          pos++;
          break;
      }
    }
    return;
  }
}
