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
#ifndef DISH_VALUE_HPP
#define DISH_VALUE_HPP
#pragma once

#include "error.hpp"

#include <string>
#include <map>
#include <vector>
#include <variant>

namespace dish::value
{
  
  template<typename... List>
  struct TypeList {};
  
  template<typename... List>
  std::variant<List...> as_variant(TypeList<List...>);
  
  template<typename T, typename List>
  struct IndexOf;
  template<typename First, typename ... Rest>
  struct IndexOf<First, TypeList<First, Rest...>>
  {
    const static int value = 0;
  };
  template<typename T>
  struct IndexOf<T, TypeList<>>
  {
    const static int value = -1;
  };
  template<typename T, typename First, typename ...Rest>
  struct IndexOf<T, TypeList<First, Rest...>>
  {
    const static int temp = IndexOf<T, TypeList<Rest...>>::value;
    const static int value = temp == -1 ? -1 : temp + 1;
  };
  
  using VTList = TypeList<std::string, int>;
  using VT = decltype(as_variant(VTList{}));
  
  enum class Attribute
  {
    read_only
  };
  
  class Value
  {
  private:
    VT value;
    Attribute attr;
  public:
    template<typename T, typename = std::enable_if_t<!std::is_base_of_v<Value, std::decay_t<T>>>>
    explicit Value(T &&data)
    {
      *this = std::forward<T>(data);
    }
    
    Value(Value &&) = default;
    
    Value(const Value &) = default;
    
    Value() : value(0) {}
    
    template<typename T>
    [[nodiscard]]T get() const
    {
      if (!is<T>())
      {
        get_error_index<T>();
      }
      return std::get<T>(value);
    }
    
    template<typename T>
    Value &operator=(T &&v)
    {
      value = std::forward<T>(v);
      return *this;
    }
    
    template<typename T>
    [[nodiscard]]bool is() const
    {
      return value.index() == IndexOf<T, VTList>::value;
    }
    template<typename T>
    void get_error_index() const
    {
      throw error::DishError(DISH_ERROR_LOCATION, __func__,
                             "Get wrong type.[wrong T = '" + IndexOf<T, VTList>::value
                             + "', correct T = '" + value.index() + "']");
    }
    [[nodiscard]] auto get_variant() const;
    [[nodiscard]] Attribute get_attr() const;
  };
}
#endif