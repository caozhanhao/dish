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
#ifndef DISH_TOKEN_HPP
#define DISH_TOKEN_HPP
#pragma once

#include "job.hpp"

#include <string>
#include <optional>
#include <vector>

namespace dish::lexer
{
  enum class TokenType
  {
    word, newline, pipe,
    lt, rt, lt_lt, lt_lt_lt, rt_rt, lt_and, rt_and, lt_rt, background,
    env_var,
    end
  };

  class Token
  {
  private:
    TokenType type;
    tiny_utf8::string content;
    std::size_t pos;
    std::size_t size;
  public:
    Token() = default;
    Token(TokenType type_, tiny_utf8::string content_, std::size_t pos_, std::size_t size_)
        : type(type_), content(std::move(content_)), pos(pos_), size(size_) {}

    TokenType get_type() const;

    tiny_utf8::string get_content() const;

    std::size_t get_pos() const;

    std::size_t get_size() const;

    void set_content(tiny_utf8::string c);
  };
}
#endif