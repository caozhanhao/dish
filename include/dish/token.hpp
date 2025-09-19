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

#include <optional>
#include <string>
#include <vector>

namespace dish::lexer
{
  enum class TokenType
  {
    error,
    word,
    newline,
    pipe,
    lt,
    rt,
    lt_lt,
    lt_lt_lt,
    rt_rt,
    lt_and,
    rt_and,
    lt_rt,
    background,
    env_var,
    end
  };

  class Token
  {
  private:
    TokenType type;
    String content;
    String error;
    std::size_t pos;
    std::size_t size;

  public:
    Token() = default;
    Token(TokenType type_, String content_, std::size_t pos_, std::size_t size_,
          String error_ = "")
        : type(type_), content(std::move(content_)), pos(pos_), size(size_), error(error_) {}

    TokenType get_type() const;

    String get_content() const;

    String get_error() const;

    std::size_t get_pos() const;

    std::size_t get_size() const;

    void set_content(String c);
    void set_error(String c);
  };
}// namespace dish::lexer
#endif