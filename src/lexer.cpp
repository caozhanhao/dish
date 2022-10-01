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
#include <string>
#include <functional>
#include <vector>

namespace dish::lexer
{
  TokenType Token::get_type() const { return type; }
  
  std::string Token::get_content() const { return content; }
  
  std::size_t Token::get_pos() const { return pos; }
  
  std::size_t Token::get_size() const { return size; }
  
  void Token::set_content(std::string c) { content = std::move(c); }
  
  
  bool is_end_of_command(CmdState cmd_state)
  {
    return (cmd_state == CmdState::word || cmd_state == CmdState::filename || cmd_state == CmdState::background);
  }
  
  void Lexer::check_cmd(const Token &token)
  {
    switch (cmd_state)
    {
      case CmdState::init:
        switch (token.get_type())
        {
          case TokenType::word:
            cmd_state = CmdState::word;
            break;
          default:
            throw error::Error("Syntax Error", "Unexpected '" + token.get_content() + "' at begin.",
                               mark_error_from_token(token));
            break;
        }
        break;
      case CmdState::word:
        switch (token.get_type())
        {
          case TokenType::pipe:
          case TokenType::lt:
          case TokenType::rt:
          case TokenType::rt_rt:
          case TokenType::rt_and:
          case TokenType::rt_rt_and:
            cmd_state = CmdState::io_modifier;
            break;
          case TokenType::word:
            cmd_state = CmdState::word;
            break;
          default:
            throw error::Error("Syntax Error", "Unexpected '" + token.get_content() + "' after a command.",
                               mark_error_from_token(token));
            break;
        }
        break;
      case CmdState::io_modifier:
        switch (token.get_type())
        {
          case TokenType::word:
            cmd_state = CmdState::filename;
            break;
          default:
            throw error::Error("Syntax Error", "Unexpected '" + token.get_content() + "' after a io_modifier.",
                               mark_error_from_token(token));
            break;
        }
        break;
      case CmdState::filename:
        switch (token.get_type())
        {
          case TokenType::pipe:
          case TokenType::lt:
          case TokenType::rt:
          case TokenType::rt_rt:
          case TokenType::rt_and:
          case TokenType::rt_rt_and:
            cmd_state = CmdState::io_modifier;
            break;
          case TokenType::background:
            cmd_state = CmdState::background;
            break;
          default:
            throw error::Error("Syntax Error", "Unexpected '" + token.get_content() + "' after a filename.",
                               mark_error_from_token(token));
            break;
        }
        break;
      case CmdState::background:
        throw error::Error("Syntax Error", "Unexpected '" + token.get_content() + "' after a background flag.",
                           mark_error_from_token(token));
        break;
    }
  }
  
  void Lexer::check_if(const Token &token)
  {
    switch (if_state)
    {
      case IfState::init:
        if_state = IfState::if_state;
        break;
      case IfState::if_state:
        if (token.get_type() == TokenType::then_tok)
        {
          if (!is_end_of_command(cmd_state))
          {
            throw error::Error("Syntax Error", "Unexpected 'then' after a incomplete command.",
                               mark_error_from_token(token));
          }
          cmd_state = CmdState::init;
          if_state = IfState::then_state;
        } else
          check_cmd(token);
        break;
      case IfState::then_state:
        if (token.get_type() == TokenType::else_tok)
        {
          if (!is_end_of_command(cmd_state))
          {
            throw error::Error("Syntax Error", "Unexpected 'else' after a incomplete command.",
                               mark_error_from_token(token));
          }
          cmd_state = CmdState::init;
          if_state = IfState::else_state;
        } else if (token.get_type() == TokenType::fi_tok)
        {
          if (!is_end_of_command(cmd_state))
          {
            throw error::Error("Syntax Error", "Unexpected 'fi' after a incomplete command.",
                               mark_error_from_token(token));
          }
          cmd_state = CmdState::init;
          if_state = IfState::fi_state;
        } else
          check_cmd(token);
        break;
      case IfState::else_state:
        if (token.get_type() == TokenType::fi_tok)
        {
          if (!is_end_of_command(cmd_state))
          {
            throw error::Error("Syntax Error", "Unexpected 'fi' after a incomplete command.",
                               mark_error_from_token(token));
          }
          cmd_state = CmdState::init;
          if_state = IfState::fi_state;
        } else
          check_cmd(token);
        break;
      case IfState::fi_state:
        throw error::Error("Syntax Error", "Unexpected '" + token.get_content() + "' after fi.",
                           mark_error_from_token(token));
        break;
    }
  }
  
  std::vector<Token> Lexer::get_all_tokens()
  {
    std::vector<Token> ret;
    cmd_state = CmdState::init;
    if_state = IfState::init;
    pos = 0;
    std::function<void(const Token &)> checker;
    
    Token t = get_token();
    
    switch (t.get_type())
    {
      case TokenType::if_tok:
        checker = [this](const Token &t) { check_if(t); };
        break;
      case TokenType::word:
        checker = [this](const Token &t) { check_cmd(t); };
        break;
    }
    checker(t);
    ret.emplace_back(t);
    while (pos < text.size())
    {
      t = get_token();
      checker(t);
      ret.emplace_back(std::move(t));
    }
    return ret;
  }
  
  Token Lexer::get_token()
  {
    while (pos < text.size() && text[pos] == ' ') ++pos;
    if (pos < text.size() && text[pos] == '\n')
      return {TokenType::newline, "\\n", pos, 0};
    else if (pos < text.size() && text[pos] == '|')
      return {TokenType::pipe, "|", pos, 1};
    else if (pos < text.size() && text[pos] == '&')
      return {TokenType::background, "&", pos, 1};
    else if (pos < text.size() && text[pos] == '<')
      return {TokenType::rt, "<", pos, 1};
    else if (pos < text.size() && text[pos] == '>')
    {
      if (pos < text.size() - 1 && text[pos + 1] == '>')
      {
        if (pos < text.size() - 2 && text[pos + 2] == '&')
        {
          return {TokenType::rt_rt_and, ">>&", pos, 3};
          pos += 2;
        } else
        {
          return {TokenType::rt_rt, ">>", pos, 2};
          ++pos;
        }
      } else if (pos < text.size() - 1 && text[pos + 1] == '&')
      {
        return {TokenType::rt_and, ">&", pos, 2};
        ++pos;
      } else
        return {TokenType::rt, ">", pos, 1};
    } else
    {
      std::string tmp;
      do
      {
        tmp += text[pos];
        ++pos;
      } while (pos < text.size() && !std::isspace(text[pos]));
      if (ids.find(tmp) == ids.cend())
        return {TokenType::word, tmp, pos, tmp.size()};
      else
        return {ids.at(tmp), tmp, pos, tmp.size()};
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
