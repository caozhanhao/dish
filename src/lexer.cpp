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

#include <string>
#include <optional>
#include <vector>

namespace dish::lexer
{
  int Lexer::check_cmd(const Token &token)
  {
    switch (cmd_state)
    {
      case CmdState::init:
        switch (token.get_type())
        {
          case TokenType::word:
          case TokenType::env_var:
            cmd_state = CmdState::word_or_env;
            break;
          case TokenType::end:
            cmd_state = CmdState::end;
            break;
          default:
            fmt::println("Syntax Error: Unexpected '{}' at begin.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::word_or_env:
        switch (token.get_type())
        {
          case TokenType::lt:
          case TokenType::rt:
          case TokenType::rt_rt:
          case TokenType::lt_lt:
          case TokenType::lt_lt_lt:
          case TokenType::lt_rt:
            cmd_state = CmdState::io_modifier_file_name;
            break;
          case TokenType::lt_and:
          case TokenType::rt_and:
            cmd_state = CmdState::io_modifier_file_desc;
            break;
          case TokenType::word:
          case TokenType::env_var:
            cmd_state = CmdState::word_or_env;
            break;
          case TokenType::pipe:
            cmd_state = CmdState::pipe;
            break;
          case TokenType::end:
            cmd_state = CmdState::end;
            break;
          case TokenType::background:
            cmd_state = CmdState::background;
            break;
          default:
            fmt::println("Syntax Error: Unexpected '{}' after a command.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::pipe:
        switch (token.get_type())
        {
          case TokenType::word:
          case TokenType::env_var:
            cmd_state = CmdState::word_or_env;
            break;
          case TokenType::end:
            fmt::println("Syntax Error: Unexpected end.");
            return -1;
            break;
          default:
            fmt::println("Syntax Error: Unexpected '{}' after a pipe.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::io_modifier_file_name:
        switch (token.get_type())
        {
          case TokenType::word:
          case TokenType::env_var:
            cmd_state = CmdState::file;
            break;
          case TokenType::pipe:
            cmd_state = CmdState::pipe;
            break;
          case TokenType::end:
            fmt::println("Syntax Error: Unexpected end.");
            return -1;
            break;
          default:
            fmt::println("Syntax Error: Unexpected '{}' after a io_modifier.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::io_modifier_file_desc:
        switch (token.get_type())
        {
          case TokenType::word:
          {
            auto fd = token.get_content();
            for(auto& r : fd)
            {
              if(!std::isdigit(r))
              {
                fmt::println("Syntax Error: Invalid file descriptor.\n{}", token.get_content(),
                             mark_error_from_token(token));
                return -1;
              }
            }
            cmd_state = CmdState::file;
          }
            break;
          case TokenType::pipe:
            cmd_state = CmdState::pipe;
            break;
          case TokenType::end:
            fmt::println("Syntax Error: Unexpected end.");
            return -1;
            break;
          default:
            fmt::println("Syntax Error: Unexpected '{}' after a io_modifier.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::file:
        switch (token.get_type())
        {
          case TokenType::lt:
          case TokenType::rt:
          case TokenType::rt_rt:
          case TokenType::lt_lt:
          case TokenType::lt_rt:
            cmd_state = CmdState::io_modifier_file_name;
            break;
          case TokenType::lt_and:
          case TokenType::rt_and:
            cmd_state = CmdState::io_modifier_file_desc;
            break;
          case TokenType::background:
            cmd_state = CmdState::background;
            break;
          case TokenType::end:
            cmd_state = CmdState::end;
            break;
          default:
            fmt::println("Syntax Error: Unexpected '{}' after a filename.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::background:
        switch (token.get_type())
        {
          case TokenType::end:
            cmd_state = CmdState::end;
            break;
          default:
            fmt::println("Syntax Error", "Unexpected '{}'after a background flag.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::end:
        fmt::println("Syntax Error", "Unexpected '{}'.\n{}", token.get_content(),
                     mark_error_from_token(token));
        return -1;
        break;
    }
    return 0;
  }

  std::optional<std::vector<Token>> Lexer::get_all_tokens_no_check()
  {
    std::vector<Token> ret;
    cmd_state = CmdState::init;
    pos = 0;
    while (pos < text.size())
    {
      auto t = get_token();
      if (t.has_value())
        ret.emplace_back(t.value());
      else
        return std::nullopt;
    }
    return ret;
  }

  std::optional<std::vector<Token>> Lexer::get_all_tokens()
  {
    std::vector<Token> ret;
    cmd_state = CmdState::init;
    pos = 0;
    while (pos < text.size())
    {
      auto t = get_token();
      if (t.has_value())
      {
        if (check_cmd(t.value()) != 0)
          return std::nullopt;
        ret.emplace_back(t.value());
      }
      else
        return std::nullopt;
    }
    if (check_cmd(Token{TokenType::end, "end", 0, 0}) != 0) return std::nullopt;
    return ret;
  }
  
  std::optional<Token> Lexer::get_token()
  {
    while (pos < text.size() && text[pos] == ' ') ++pos;
    if (pos < text.size() && text[pos] == '\n')
      return Token{TokenType::newline, "\\n", pos++, 0};
    else if (pos < text.size() && text[pos] == '|')
      return Token{TokenType::pipe, "|", pos++, 1};
    else if (pos < text.size() && text[pos] == '&')
      return Token{TokenType::background, "&", pos++, 1};
    else if (pos < text.size() && text[pos] == '<')
    {
      if (pos < text.size() - 1 && text[pos + 1] == '<')
      {
        if (pos < text.size() - 2 && text[pos + 2] == '<')
          return Token{TokenType::lt_lt_lt, "<<<", pos += 3, 3};
        else
          return Token{TokenType::lt_lt, "<<", pos += 2, 2};
      }
      else if (pos < text.size() - 1 && text[pos + 1] == '&')
          return Token{TokenType::lt_and, "<&", pos += 2, 2};
      else if (pos < text.size() - 1 && text[pos + 1] == '>')
        return Token{TokenType::lt_rt, "<>", pos += 2, 2};
      else
        return Token{TokenType::lt, "<", pos++, 1};
    }
    else if (pos < text.size() && text[pos] == '>')
    {
      if (pos < text.size() - 1 && text[pos + 1] == '>')
        return Token{TokenType::rt_rt, ">>", pos += 2, 2};
      else if (pos < text.size() - 1 && text[pos + 1] == '&')
        return Token{TokenType::rt_and, ">&", pos += 2, 2};
      else
        return Token{TokenType::rt, ">", pos++, 1};
    }
    else if (pos < text.size() && text[pos] == '$')
    {
      ++pos;
      std::string tmp;
      if(text[pos] == '{')
      {
        ++pos;
        if(pos == text.size())
        {
          fmt::println("Synax Error: Unexpected end of token.");
          return std::nullopt;
        }
        do
        {
          tmp += text[pos];
          ++pos;
        } while (pos < text.size() && text[pos] != '}');
        if(pos == text.size() || text[pos] != '}')
        {
          fmt::println("Synax Error: Unexpected end of token.");
          return std::nullopt;
        }
        pos++;//skip '}'
      }
      else
      {
        do
        {
          tmp += text[pos];
          ++pos;
        } while (pos < text.size() && !std::isspace(text[pos]));
      }
      return Token{TokenType::env_var, tmp, pos, tmp.size()};
    }
    else
    {
      std::string tmp;
      do
      {
        tmp += text[pos];
        ++pos;
      } while (pos < text.size() && !std::isspace(text[pos]));
      return Token{TokenType::word, tmp, pos, tmp.size()};
    }
  }
  
  std::string Lexer::mark_error_from_token(const Token &token) const
  {
    std::size_t size = token.get_size();
    std::size_t errpos = token.get_pos();
    std::string marked = text;
    marked += "\n";
    if (errpos >= size)
    {
      marked += std::string(errpos - size, ' ');
    }
    marked += "\033[0;32;32m";
    marked += std::string(size, '^');
    marked += "\033[m\n";
    return marked;
  }
}
