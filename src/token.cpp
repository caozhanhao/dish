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

#include "dish/token.hpp"

#include <string>

namespace dish::lexer
{
  TokenType Token::get_type() const { return type; }
  String Token::get_content() const { return content; }
  String Token::get_error() const { return error; }
  std::size_t Token::get_pos() const { return pos; }
  std::size_t Token::get_size() const { return size; }
  void Token::set_content(String c)
  {
    content = std::move(c);
  }
  void Token::set_error(String c)
  {
    error = std::move(c);
  }
}// namespace dish::lexer
