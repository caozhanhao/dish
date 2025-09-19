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

#include "dish/lexer.hpp"
#include "dish/utils.hpp"

#include <optional>
#include <string>
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
            fmt::println(stderr, "Syntax Error: Unexpected '{}' at begin.\n{}", token.get_content(),
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
            fmt::println(stderr, "Syntax Error: Unexpected '{}' after a command.\n{}", token.get_content(),
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
            fmt::println(stderr, "Syntax Error: Unexpected end.");
            return -1;
            break;
          default:
            fmt::println(stderr, "Syntax Error: Unexpected '{}' after a pipe.\n{}", token.get_content(),
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
            fmt::println(stderr, "Syntax Error: Unexpected end.");
            return -1;
            break;
          default:
            fmt::println(stderr, "Syntax Error: Unexpected '{}' after a io_modifier.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::io_modifier_file_desc:
        switch (token.get_type())
        {
          case TokenType::word: {
            auto fd = token.get_content();
            for (auto r: fd)
            {
              if (!std::isdigit(r))
              {
                fmt::println(stderr, "Syntax Error: Invalid file descriptor.\n{}", token.get_content(),
                             mark_error_from_token(token));
                return -1;
              }
            }
            cmd_state = CmdState::file;
          } break;
          case TokenType::pipe:
            cmd_state = CmdState::pipe;
            break;
          case TokenType::end:
            fmt::println(stderr, "Syntax Error: Unexpected end.");
            return -1;
            break;
          default:
            fmt::println(stderr, "Syntax Error: Unexpected '{}' after a io_modifier.\n{}", token.get_content(),
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
            fmt::println(stderr, "Syntax Error: Unexpected '{}' after a filename.\n{}", token.get_content(),
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
            fmt::println(stderr, "Syntax Error: Unexpected '{}' after a background flag.\n{}", token.get_content(),
                         mark_error_from_token(token));
            return -1;
            break;
        }
        break;
      case CmdState::end:
        fmt::println(stderr, "Syntax Error: Unexpected '{}'.\n{}", token.get_content(),
                     mark_error_from_token(token));
        return -1;
        break;
    }
    return 0;
  }

  std::vector<Token> Lexer::get_all_tokens_no_check()
  {
    std::vector<Token> ret;
    cmd_state = CmdState::init;
    pos = 0;
    while (pos < text.length())
    {
      auto t = get_token();
      ret.emplace_back(t);
    }
    return ret;
  }

  std::optional<std::vector<Token>> Lexer::get_all_tokens()
  {
    std::vector<Token> ret;
    cmd_state = CmdState::init;
    pos = 0;
    while (pos < text.length())
    {
      auto t = get_token();
      if (t.get_type() == TokenType::error)
      {
        fmt::println(stderr, t.get_content().cpp_str());
        return std::nullopt;
      }
      if (check_cmd(t) != 0)
        return std::nullopt;
      if (t.get_type() != TokenType::end)
        ret.emplace_back(t);
    }
    if (cmd_state != CmdState::end)
    {
      if (check_cmd(Token{TokenType::end, "end", 0, 0}) != 0)
        return std::nullopt;
    }
    return ret;
  }

  Token Lexer::get_token()
  {
    while (pos < text.length() && text[pos] == ' ') ++pos;
    if (pos < text.length() && text[pos] == '\n')
      return Token{TokenType::newline, "\\n", ++pos, 0};
    else if (pos < text.length() && text[pos] == '|')
      return Token{TokenType::pipe, "|", pos += 1, 1};
    else if (pos < text.length() && text[pos] == '&')
      return Token{TokenType::background, "&", ++pos, 1};
    else if (pos < text.length() && text[pos] == '<')
    {
      if (pos < text.length() - 1 && text[pos + 1] == '<')
      {
        if (pos < text.length() - 2 && text[pos + 2] == '<')
          return Token{TokenType::lt_lt_lt, "<<<", pos += 3, 3};
        else
          return Token{TokenType::lt_lt, "<<", pos += 2, 2};
      }
      else if (pos < text.length() - 1 && text[pos + 1] == '&')
        return Token{TokenType::lt_and, "<&", pos += 2, 2};
      else if (pos < text.length() - 1 && text[pos + 1] == '>')
        return Token{TokenType::lt_rt, "<>", pos += 2, 2};
      else
        return Token{TokenType::lt, "<", ++pos, 1};
    }
    else if (pos < text.length() && text[pos] == '>')
    {
      if (pos < text.length() - 1 && text[pos + 1] == '>')
        return Token{TokenType::rt_rt, ">>", pos += 2, 2};
      else if (pos < text.length() - 1 && text[pos + 1] == '&')
        return Token{TokenType::rt_and, ">&", pos += 2, 2};
      else
        return Token{TokenType::rt, ">", ++pos, 1};
    }
    else if (pos < text.length() && text[pos] == '$')
    {
      ++pos;
      String tmp = "$";
      if (text[pos] == '{')
      {
        tmp += "{";
        ++pos;
        if (pos == text.length())
          return {TokenType::error, tmp, pos, tmp.length(),
                  "Syntax Error: Unexpected end of token."};

        while (pos < text.length() && text[pos] != '}')
        {
          tmp += text[pos];
          ++pos;
        }

        if (pos == text.length() || text[pos] != '}')
        {
          return {TokenType::error, tmp, pos, tmp.length(),
                  "Syntax Error: Unexpected end of token."};
        }
        tmp += "}";
        pos++;//skip '}'
      }
      else
      {
        while (pos < text.length() && !std::isspace(text[pos]))
        {
          tmp += text[pos];
          ++pos;
        }
      }
      if (tmp.length() == 1)
      {
        return {TokenType::error, tmp, pos, tmp.length(),
                "Syntax Error: Unexpected end of token."};
      }
      return Token{TokenType::env_var, tmp, pos, tmp.length()};
    }
    else if (pos >= text.length())
      return Token{TokenType::end, "end", 0, 0};
    else
    {
      String tmp;
      bool parsing_str = false;
      while (pos < text.length() && (parsing_str || text[pos] != ' '))
      {
        if (text[pos] == '"')
        {
          parsing_str = !parsing_str;
          tmp += '"';
        }
        else
          tmp += text[pos];
        ++pos;
      }
      if (parsing_str)
        return Token{TokenType::error, tmp, pos, tmp.length(),
                     "Syntax Error: Unexpected end of token."};
      return Token{TokenType::word, tmp, pos, tmp.length()};
    }
    return Token{TokenType::end, "end", 0, 0};
  }

  String Lexer::mark_error_from_token(const Token &token) const
  {
    std::size_t size = token.get_size();
    std::size_t errpos = token.get_pos();
    String marked = text;
    marked += "\n";
    if (errpos >= size)
      marked += String(errpos - size, ' ');
    marked += "\033[0;32;32m";
    marked += String(size, '^');
    marked += "\033[m";
    return marked;
  }
}// namespace dish::lexer