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
#ifndef DISH_LEXER_HPP
#define DISH_LEXER_HPP
#pragma once

#include "error.hpp"
#include "command.hpp"

#include <string>
#include <optional>
#include <vector>

namespace dish::lexer
{
  enum class TokenType
  {
    word, lt, rt, newline, pipe,
    rt_and, rt_rt, rt_rt_and, background,
    end
  };
  enum class CmdState
  {
    init, word, io_modifier, pipe,  filename, background, end
  };
  
  class Token
  {
  private:
    TokenType type;
    std::string content;
    std::size_t pos;
    std::size_t size;
  public:
    Token() = default;
    Token(TokenType type_, std::string content_, std::size_t pos_, std::size_t size_)
        : type(type_), content(std::move(content_)), pos(pos_), size(size_) {}
    
    TokenType get_type() const;
    
    std::string get_content() const;
    
    std::size_t get_pos() const;
    
    std::size_t get_size() const;
    
    void set_content(std::string c);
  };
  
  class Lexer
  {
  private:
    std::string text;
    std::size_t pos;
    CmdState cmd_state;
  public:
    Lexer(std::string cmd) : text(cmd), pos(0) {}
  
    std::optional<std::vector<Token>> get_all_tokens();
    
  private:
    Token get_token();
    
    int check_cmd(const Token &token);
    
    std::string mark_error_from_token(const Token &token) const;
  };
}
#endif