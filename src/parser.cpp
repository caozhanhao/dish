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
#include "dish/lexer.hpp"
#include "dish/command.hpp"
#include "dish/parser.hpp"

#include <string>
#include <vector>

namespace dish::parser
{
  
  Parser::Parser(DishInfo *info_, std::vector<lexer::Token> tokens_)
      : info(info_), scmd(nullptr), command(info_), tokens(tokens_), pos(0) {}
  
  int Parser::parse()
  {
    pos = 0;
    while (pos < tokens.size())
    {
      if(parse_cmd(command) == -1)
        return -1;
    }
    return 0;
  }
  
  cmd::Command Parser::get_cmd() const { return command; }
  
  int Parser::parse_cmd(cmd::Command &cmd)
  {
    auto add_scmd = [&cmd, this]()
    {
      cmd::SimpleCmd scmd;
      while (pos < tokens.size() &&
      (tokens[pos].get_type() == lexer::TokenType::word || tokens[pos].get_type() == lexer::TokenType::env_var))
      {
        auto content = tokens[pos++].get_content();
        if (tokens[pos - 1].get_type() == lexer::TokenType::word && utils::has_wildcards(content))
        {
          auto expanded = utils::expand_wildcards(content);
          if(!expanded.has_value())
            return -1;
          for(auto& r : expanded.value())
            scmd.insert(r);
        }
        else if(tokens[pos - 1].get_type() == lexer::TokenType::env_var)
        {
          auto env = utils::expand_env_var(content);
          if(!env.has_value())
          {
            fmt::println("Unknown environment variable.");
            return -1;
          }
          scmd.insert(env.value());
        }
        else
          scmd.insert(content);
      }
      cmd.insert(std::make_shared<cmd::SimpleCmd>(scmd));
      return 0;
    };
    if(add_scmd() == -1) return -1;
    while (pos < tokens.size() && tokens[pos].get_type() == lexer::TokenType::pipe)
    {
      ++pos;
      if(add_scmd() == -1) return -1;
    }
  
    while (pos < tokens.size())
    {
      switch (tokens[pos].get_type())
      {
        case lexer::TokenType::lt://<
          cmd.set_in(cmd::Redirect{cmd::RedirectType::input, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::rt://>
          cmd.set_out(cmd::Redirect{cmd::RedirectType::overwrite, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::lt_lt://<<
          fmt::println("TODO");
          pos += 2;
          break;
        case lexer::TokenType::lt_lt_lt://<<<
          fmt::println("TODO");
          pos += 2;
          break;
        case lexer::TokenType::rt_rt://>>
          cmd.set_out(cmd::Redirect{cmd::RedirectType::append, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::lt_and://<&
          cmd.set_in(cmd::Redirect{cmd::RedirectType::fd, std::stoi(tokens[pos + 1].get_content())});
          pos += 2;
          break;
        case lexer::TokenType::rt_and://>&
          cmd.set_out(cmd::Redirect{cmd::RedirectType::fd, std::stoi(tokens[pos + 1].get_content())});
          pos += 2;
          break;
        case lexer::TokenType::lt_rt://<>
          fmt::println("TODO");
          pos += 2;
          break;
        case lexer::TokenType::background://&
          cmd.set_background();
          pos++;
          break;
      }
    }
    return 0;
  }
}
