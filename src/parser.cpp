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
#include "command.h"
#include "parser.h"
#include <string>
#include <vector>

namespace dish::parser
{
  TokenType Token::get_type() const { return type; }
  
  std::string Token::get_content() const { return content; }
  
  std::size_t Token::get_pos() const { return pos; }
  
  std::size_t Token::get_size() const { return size; }
  
  void Token::set_content(std::string c) { content = std::move(c); }
  
  Parser::Parser(Dish *dish_, std::string cmd) : text(std::move(cmd)), state(State::init), pos(0), dish(dish_) {}
  
  void Parser::parse()
  {
    std::vector<Token> tokens{get_tokens()};
    for (auto &t: tokens)
    {
      parse_token(t);
    }
    if (!tmp.empty())
    {
      command.insert(tmp);
    }
    command.set_dish(dish);
  }
  
  cmd::Command Parser::get_cmd() const { return command; }
  
  std::vector<Token> Parser::get_tokens() const
  {
    std::vector<Token> tokens;
    for (auto it = text.cbegin(); it < text.cend(); ++it)
    {
      auto &ch = *it;
      if (ch == ' ') continue;
      if (ch == '\n')
      {
        tokens.emplace_back(TokenType::newline, "\n", it - text.cbegin(), 0);
        continue;
      }
      else if (ch == '|')
      {
        tokens.emplace_back(TokenType::pipe, "", it - text.cbegin(), 1);
        continue;
      }
      else if (ch == '&')
      {
        tokens.emplace_back(TokenType::background, "", it - text.cbegin(), 1);
        continue;
      }
      else if (ch == '<')
      {
        tokens.emplace_back(TokenType::input, "", it - text.cbegin(), 1);
        continue;
      }
      else if (ch == '>')
      {
        if (it < text.cend() - 1 && *(it + 1) == '>')
        {
          if (it < text.cend() - 2 && *(it + 2) == '&')
          {
            tokens.emplace_back(TokenType::great2_ampersand, "", it - text.cbegin(), 3);
            it += 2;
          }
          else
          {
            tokens.emplace_back(TokenType::appending, "", it - text.cbegin(), 2);
            ++it;
          }
        }
        else if (it < text.cend() - 1 && *(it + 1) == '&')
        {
          tokens.emplace_back(TokenType::great_ampersand, "", it - text.cbegin(), 2);
          ++it;
        }
        else
        {
          tokens.emplace_back(TokenType::output, "", it - text.cbegin(), 1);
        }
        continue;
      }
      else
      {
        std::string tmp;
        do
        {
          tmp += *it;
          ++it;
        } while (it < text.cend() && !std::isspace(*it));
        std::size_t sz = tmp.size();
        if (!tokens.empty() && tokens.back().get_type() != TokenType::word)
        {
          tokens.back().set_content(tmp);
        }
        else
        {
          tokens.emplace_back(TokenType::word, tmp, it - text.cbegin(), sz);
        }
      }
    }
    return tokens;
  }
  
  void Parser::parse_token(const Token &token)
  {
    switch (state)
    {
      case State::init:
        switch (token.get_type())
        {
          case TokenType::word:
            state = State::command;
            tmp.insert(token.get_content());
            break;
          default:
            throw error::Error("Syntax Error", "Unexpected token.", mark_error_from_token(token));
            break;
        }
        break;
      case State::command:
        switch (token.get_type())
        {
          case TokenType::word:
            tmp.insert(token.get_content());
            break;
          case TokenType::pipe:
            command.insert(tmp);
            tmp.clear();
            tmp.insert(token.get_content());
            break;
          case TokenType::output:
            command.set_out({cmd::RedirectType::output, token.get_content()});
            break;
          case TokenType::appending:
            command.set_out({cmd::RedirectType::appending, token.get_content()});
            break;
          case TokenType::input:
            command.set_in({cmd::RedirectType::input, token.get_content()});
            break;
          case TokenType::background:
            command.set_background();
            break;
          default:
            throw error::Error("Syntax Error", "Unexpected token.", mark_error_from_token(token));
            break;
        }
        break;
      default:
        break;
    }
  }
  
  std::string Parser::mark_error_from_token(const Token &token) const
  {
    std::size_t size = token.get_size();
    std::size_t errpos = token.get_pos();
    std::string marked = text;
    marked += "\n";
    if (errpos >= size)
    {
      marked += std::string(errpos - size + 1, ' ');
    }
    marked += "\033[0;32;32m";
    marked += std::string(size, '^');
    marked += "\033[m\n";
    return marked;
  }
  
}
