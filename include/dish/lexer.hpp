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
#ifndef DISH_LEXER_HPP
#define DISH_LEXER_HPP
#pragma once

#include "job.hpp"
#include "token.hpp"

#include <optional>
#include <string>
#include <vector>

namespace dish::lexer
{
  enum class CmdState
  {
    init,
    word_or_env,
    io_modifier_file_name,
    io_modifier_file_desc,
    pipe,
    file,
    background,
    end
  };

  class Lexer
  {
  private:
    String text;
    std::size_t pos;
    CmdState cmd_state;

  public:
    Lexer(String cmd) : text(cmd), pos(0) {}

    std::optional<std::vector<Token>> get_all_tokens();

    std::vector<Token> get_all_tokens_no_check();

  private:
    Token get_token();

    int check_cmd(const Token &token);

    String mark_error_from_token(const Token &token) const;
  };
}// namespace dish::lexer
#endif