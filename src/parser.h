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
#include "command.h"
#include <string>
#include <vector>

namespace dish::parser
{
  enum class TokenType
  {
    output, input, appending, newline, word, pipe,
    great_ampersand, great2_ampersand, background
  };
  enum class State
  {
    init, command
  };
  
  class Token
  {
  private:
    TokenType type;
    std::string content;
    std::size_t pos;
    std::size_t size;
  public:
    Token(TokenType type_, std::string content_, std::size_t pos_, std::size_t size_)
        : type(type_), content(std::move(content_)), pos(pos_), size(size_) {}
    
    TokenType get_type() const;
    
    std::string get_content() const;
    
    std::size_t get_pos() const;
    
    std::size_t get_size() const;
    
    void set_content(std::string c);
  };
  
  class Parser
  {
  private:
    std::string text;
    cmd::Command command;
    cmd::SingleCommand tmp;
    State state;
    std::size_t pos;
    Dish *dish;
  public:
    Parser(Dish *dish_, std::string cmd);
    
    void parse();
    
    cmd::Command get_cmd() const;
  
  private:
    std::vector<Token> get_tokens() const;
    
    void parse_token(const Token &token);
    
    std::string mark_error_from_token(const Token &token) const;
  };
}
#endif