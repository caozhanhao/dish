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
#include "error.h"
#include "lexer.h"
#include "command.h"
#include "parser.h"
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
      switch (tokens[pos].get_type())
      {
        case lexer::TokenType::word:
          parse_cmd(command);
          break;
        case lexer::TokenType::if_tok:
          parse_if();
          break;
      }
    }
  }
  
  cmd::Command Parser::get_cmd() const { return command; }
  
  void Parser::parse_if()
  {
    ++pos;//skip if
    cmd::Command condition{info};
    parse_cmd(condition);
    ++pos;//skip then
    cmd::Command true_case{info};
    cmd::Command false_case{info};
    parse_cmd(true_case);
    if (tokens[pos].get_type() == lexer::TokenType::else_tok)
    {
      ++pos;
      parse_cmd(false_case);
    }
    ++pos;//fi
    command.insert(std::make_shared<cmd::IfCmd>(
        std::make_shared<cmd::Command>(condition),
        std::make_shared<cmd::Command>(true_case),
        std::make_shared<cmd::Command>(false_case)));
  }
  
  void Parser::parse_cmd(cmd::Command &cmd)
  {
    auto add_scmd = [&cmd, this]()
    {
      cmd::SimpleCmd scmd;
      while (pos < tokens.size() && tokens[pos].get_type() == lexer::TokenType::word)
      {
        scmd.insert(tokens[pos].get_content());
        ++pos;
      }
      cmd.insert(std::make_shared<cmd::SimpleCmd>(scmd));
    };
    add_scmd();
    while (pos < tokens.size() && tokens[pos].get_type() == lexer::TokenType::pipe)
      add_scmd();
    
    auto io_modify = [&cmd, this]()
    {
      while (pos < tokens.size())
      {
        switch (tokens[pos].get_type())
        {
          case lexer::TokenType::rt:
            cmd.set_in(cmd::Redirect{cmd::RedirectType::input, tokens[pos + 1].get_content()});
            pos += 2;
            break;
          case lexer::TokenType::rt_and:
            cmd.set_in(cmd::Redirect{cmd::RedirectType::appending, tokens[pos + 1].get_content()});
            pos += 2;
            break;
          case lexer::TokenType::background:
            cmd.set_background();
            pos++;
            break;
          default:
            return;
        }
      }
    };
    io_modify();
  }
  
}
