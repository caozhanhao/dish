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

#include "dish/parser.hpp"
#include "dish/job.hpp"
#include "dish/lexer.hpp"
#include "dish/utils.hpp"

#include <string>
#include <vector>

namespace dish::parser
{
  Parser::Parser(const String &cmd, std::vector<lexer::Token> tokens_) : command(cmd), tokens(tokens_), pos(0) {}

  int Parser::parse()
  {
    pos = 0;
    while (pos < tokens.size())
    {
      if (parse_cmd(command) == -1) return -1;
    }
    return 0;
  }

  job::Job Parser::get_cmd() const { return command; }

  int Parser::parse_cmd(job::Job &cmd)
  {
    auto add_scmd = [&cmd, this]() {
      job::Process scmd;
      while (pos < tokens.size() && (tokens[pos].get_type() == lexer::TokenType::word ||
                                     tokens[pos].get_type() == lexer::TokenType::env_var)) {
        auto content = tokens[pos++].get_content();
        if (content.starts_with('"') && content.back() == '"' && content.size() > 2)
          content = content.substr(1, content.size() - 2);

        // Substitute alias
        if (scmd.empty())
        {
          auto it = dish_context.lua_state["dish"]["alias"][content.cpp_str()];
          if (it.valid())
          {
            tokens.erase(tokens.begin() + pos - 1);
            auto alias = lexer::Lexer(it.get<std::string>()).get_all_tokens_no_check();
            content = alias[0].get_content();
            tokens.insert(tokens.begin() + pos - 1, std::make_move_iterator(alias.begin()),
                          std::make_move_iterator(alias.end()));
          }
        }

        // glob and ~
        if (tokens[pos - 1].get_type() == lexer::TokenType::word)
        {
          auto expanded = utils::expand(content);
          for (auto &r: expanded)
            scmd.insert(r);
        }
        // environment variable
        else if (tokens[pos - 1].get_type() == lexer::TokenType::env_var)
          scmd.insert(utils::get_dish_env(content));
        else
          scmd.insert(content);
      }
      cmd.insert(scmd);
      return 0;
    };
    if (add_scmd() == -1) return -1;
    while (pos < tokens.size() && tokens[pos].get_type() == lexer::TokenType::pipe)
    {
      ++pos;
      if (add_scmd() == -1) return -1;
    }

    while (pos < tokens.size())
    {
      switch (tokens[pos].get_type())
      {
        case lexer::TokenType::lt://<
          cmd.set_in(job::Redirect{job::RedirectType::input, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::rt://>
          cmd.set_out(job::Redirect{job::RedirectType::overwrite, tokens[pos + 1].get_content()});
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
          cmd.set_out(job::Redirect{job::RedirectType::append, tokens[pos + 1].get_content()});
          pos += 2;
          break;
        case lexer::TokenType::lt_and://<&
          cmd.set_in(job::Redirect{job::RedirectType::fd, std::stoi(tokens[pos + 1].get_content().cpp_str())});
          pos += 2;
          break;
        case lexer::TokenType::rt_and://>&
          cmd.set_out(job::Redirect{job::RedirectType::fd, std::stoi(tokens[pos + 1].get_content().cpp_str())});
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
}// namespace dish::parser