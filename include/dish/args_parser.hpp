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
#ifndef DISH_ARGS_PARSER_HPP
#define DISH_ARGS_PARSER_HPP
#pragma once

#include "utils.hpp"

#include <vector>
#include <functional>
#include <regex>
#include <map>
#include <optional>
#include <string>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <variant>
#include <filesystem>

namespace dish::args
{
  template<typename T>
  struct Restrictor
  {
    std::function<bool(const T &)> restrictor;
    std::string description;

    Restrictor(std::function<bool(const T &)> restrictor_, std::string description_)
        : restrictor(std::move(restrictor_)), description(std::move(description_))
    {}

    Restrictor()
    : description("default restrictor"), restrictor([](const T &){return true;})
    {}

    bool is_valid(const T &value)
    {
      if (restrictor)
        return restrictor(value);
      return true;
    }
  };
  template<typename T>
  Restrictor<T> range(T a, T b)
  {
    return {[a, b](const T &v) -> bool { return v >= a && v < b; },
            fmt::format("{} <= x < {}", a, b)};
  }

  template<typename T>
  Restrictor<typename T::value_type> oneof(const T &s)
  {
    return {[s](const typename T::value_type &v) -> bool {
              return std::find(std::cbegin(s), std::cend(s), v) != std::cend(s);
            },
            fmt::format("one of {}", fmt::join(s, ", "))};
  }

  template<typename T>
  Restrictor<T> oneof(const std::initializer_list<T> &s)
  {
    return {[s](const T &v) -> bool { return std::find(std::cbegin(s), std::cend(s), v) != std::cend(s); },
            fmt::format("one of {}", fmt::join(s, ", "))};
  }

  Restrictor<std::string> existing_path()
  {
    return {[](const std::string &v) -> bool {
              return std::filesystem::exists(v);
            },
            "an existing path"};
  }

  Restrictor<std::string> regex(const std::string &pattern)
  {
    return {[pattern](const std::string &v) -> bool {
              return std::regex_match(v, std::regex{pattern});
            },
            pattern};
  }

  Restrictor<std::string> email()
  {
    return {regex("^\\w+([-+.]\\w+)*@\\w+([-.]\\w+)*\\.\\w+([-.]\\w+)*$").restrictor,
            "email"};
  }

  template<typename T>
  std::optional<T> str_to(const std::string &s);

  template<>
  std::optional<std::string> str_to(const std::string &s)
  {
    return s;
  }

  template<>
  std::optional<double> str_to(const std::string &s)
  {
    try
    {
      return std::stod(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to double.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<int> str_to(const std::string &s)
  {
    try
    {
      return std::stoi(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to int.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<long> str_to(const std::string &s)
  {
    try
    {
      return std::stol(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to long.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<long long> str_to(const std::string &s)
  {
    try
    {
      return std::stoll(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to long long.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<long double> str_to(const std::string &s)
  {
    try
    {
      return std::stold(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to long double.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<unsigned long> str_to(const std::string &s)
  {
    try
    {
      return std::stoul(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to unsigned long.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<unsigned long long> str_to(const std::string &s)
  {
    try
    {
      return std::stoull(s);
    } catch (...)
    {
      fmt::println(stderr, "Warning: Unknown conversion of '{}' to unsigned long long.", s);
      return std::nullopt;
    }
    return std::nullopt;
  }

  template<>
  std::optional<bool> str_to(const std::string &s)
  {
    if (s == "true" || s == "True" || s == "TRUE")
      return true;
    else if (s == "false" || s == "False" || s == "FALSE")
      return false;
    fmt::println(stderr, "Warning: Unknown conversion of '{}' to boolean.", s);
    return std::nullopt;
  }

  template<typename... List>
  struct TypeList
  {
  };

  struct TypeListError
  {
  };

  template<typename... List>
  std::variant<List...> as_variant(TypeList<List...>);

  template<typename... List>
  std::variant<Restrictor<List>...> as_restrictor_variant(TypeList<List...>);

  template<typename List1, typename List2>
  struct link;
  template<typename... Args1, typename... Args2>
  struct link<TypeList<Args1...>, TypeList<Args2...>>
  {
    using type = TypeList<Args1..., Args2...>;
  };

  template<typename L1, typename L2>
  using link_t = typename link<L1, L2>::type;

  template<typename T, typename List>
  struct contains : std::true_type
  {
  };
  template<typename T, typename First, typename... Rest>
  struct contains<T, TypeList<First, Rest...>>
      : std::conditional<std::is_same_v<T, First>, std::true_type,
                         contains<T, TypeList<Rest...>>>::type
  {
  };
  template<typename T>
  struct contains<T, TypeList<>> : std::false_type
  {
  };

  template<typename T, typename List>
  constexpr bool contains_v = contains<T, List>::value;

  template<typename T, typename List>
  struct index_of;
  template<typename First, typename... Rest>
  struct index_of<First, TypeList<First, Rest...>>
  {
    static constexpr int value = 0;
  };
  template<typename T>
  struct index_of<T, TypeList<>>
  {
    static constexpr int value = -1;
  };
  template<typename T, typename First, typename... Rest>
  struct index_of<T, TypeList<First, Rest...>>
  {
    static constexpr int temp = index_of<T, TypeList<Rest...>>::value;
    static constexpr int value = temp == -1 ? -1 : temp + 1;
  };

  template<typename T, typename List>
  constexpr int index_of_v = index_of<T, List>::value;

  template<int index, typename List>
  struct index_at;
  template<int index>
  struct index_at<index, TypeList<>>
  {
    using type = TypeListError;
  };
  template<typename First, typename... Rest>
  struct index_at<0, TypeList<First, Rest...>>
  {
    using type = First;
  };
  template<int index, typename First, typename... Rest>
  struct index_at<index, TypeList<First, Rest...>>
  {
    using type = typename index_at<index - 1, TypeList<Rest...>>::type;
  };

  template<int index, typename List>
  using index_at_t = typename index_at<index, List>::type;

  template<typename List, size_t sz = 0>
  struct size_of;
  template<size_t sz>
  struct size_of<TypeList<>, sz>
  {
    static constexpr size_t value = sz;
  };
  template<typename First, typename... Rest, size_t sz>
  struct size_of<TypeList<First, Rest...>, sz>
  {
    static constexpr size_t value = size_of<TypeList<Rest...>>::value + 1;
  };

  template<typename List>
  constexpr size_t size_of_v = size_of<List>::value;

  using ArgsTypeList = TypeList<std::monostate, bool, int, double, std::string>;
  using ArgsType = decltype(as_variant(ArgsTypeList{}));
  using RestrictorVariant = decltype(as_restrictor_variant(ArgsTypeList{}));

  std::string get_typename(int index)
  {
    if (index == -1 || index >= size_of_v<ArgsTypeList>) return "unknown";
    static std::vector names{"null", "bool", "int", "double", "std::string"};
    return names[index];
  }
  template<typename T>
  std::string get_restrictor_description_helper(const RestrictorVariant &restrictors)
  {
    if (restrictors.index() == 0 || restrictors.valueless_by_exception())
      return "";
    auto &restrictor = std::get<Restrictor<T>>(restrictors);
    if (!restrictor.description.empty())
      return restrictor.description;
    return "";
  }
  std::string get_restrictor_description(const std::vector<RestrictorVariant> &restrictors)
  {
    std::string ret;
    for (auto &restrictor: restrictors)
    {
      switch (restrictor.index())
      {
        case index_of_v<bool, ArgsTypeList>:
          ret += get_restrictor_description_helper<bool>(restrictor);
          break;
        case index_of_v<int, ArgsTypeList>:
          ret += get_restrictor_description_helper<int>(restrictor);
          break;
        case index_of_v<double, ArgsTypeList>:
          ret += get_restrictor_description_helper<double>(restrictor);
          break;
        case index_of_v<std::string, ArgsTypeList>:
          ret += get_restrictor_description_helper<std::string>(restrictor);
          break;
      }
      ret += ", ";
    }
    if (!ret.empty())
    {
      ret.pop_back();
      ret.pop_back();
    }
    return ret;
  }
  std::string to_str(const ArgsType &val)
  {
    if (val.valueless_by_exception()) return "valueless";
    switch (val.index())
    {
      case index_of_v<std::monostate, ArgsTypeList>:
        return "null";
        break;
      case index_of_v<bool, ArgsTypeList>:
        return std::get<bool>(val) ? "true" : "false";
        break;
      case index_of_v<int, ArgsTypeList>:
        return std::to_string(std::get<int>(val));
        break;
      case index_of_v<double, ArgsTypeList>:
        return std::to_string(std::get<double>(val));
        break;
      case index_of_v<std::string, ArgsTypeList>:
        return std::get<std::string>(val);
        break;
    }
    return "unexpected";
  }

  class ArgsParser
  {
  private:
    struct Option
    {
      std::string long_name;
      std::string short_name;
      std::string description;

      int expected_type;
      std::vector<RestrictorVariant> restrictors;
      ArgsType value;
      ArgsType default_value;


      std::function<void()> func;

      Option(int expected_type_, std::vector<RestrictorVariant> restrictors_, std::string long_name_, std::string short_name_,
             ArgsType default_value = std::monostate{})
          : expected_type(expected_type_), short_name(std::move(short_name_)),
            long_name(std::move(long_name_)),
            default_value(std::move(default_value)),
            restrictors(restrictors_)
      {}

      Option(std::string long_name_, std::string short_name_,
             std::function<void()> func_)
          : short_name(std::move(short_name_)),
            long_name(std::move(long_name_)),
            func(std::move(func_)),
            expected_type(0)
      {}

      template<typename T>
      bool has() const
      {
        return (value.index() == index_of_v<std::decay_t<T>, ArgsTypeList>) || (default_value.index() == index_of_v<std::decay_t<T>, ArgsTypeList>);
      }
      template<typename T>
      std::optional<T> get()
      {
        if (has<T>())
        {
          if (!value.valueless_by_exception())
            return std::get<T>(value);
          else
            return std::get<T>(default_value);
        }
        return std::nullopt;
      }
      Option &add_description(const std::string &description_)
      {
        description = description_;
        return *this;
      }
      Option &add_restrictor(const RestrictorVariant &restrictor_)
      {
        restrictors.emplace_back(restrictor_);
        return *this;
      }
    };
    std::vector<Option> options;
    std::map<std::string, size_t> long_name_index;
    std::map<std::string, size_t> short_name_index;
    std::string name;
    std::string description;

  public:
    template<typename T>
    Option &add_option(const std::string &long_name, const std::string &short_name = "",
                       const ArgsType &default_value = std::monostate{})
    {
      static_assert(contains_v<std::decay_t<T>, ArgsTypeList> && !std::is_same_v<std::monostate, T>, "Unsupported Type.");
      options.emplace_back(Option(index_of_v<std::decay_t<T>, ArgsTypeList>, {},
                                  long_name, short_name, default_value));
      long_name_index[long_name] = options.size() - 1;
      if (!short_name.empty())
        short_name_index[short_name] = options.size() - 1;
      return options.back();
    }

    Option &add_boolean_option(const std::string &long_name, const std::string &short_name = "")
    {
      options.emplace_back(Option(index_of_v<bool, ArgsTypeList>, {},
                                  long_name, short_name, true));
      long_name_index[long_name] = options.size() - 1;
      if (!short_name.empty())
        short_name_index[short_name] = options.size() - 1;
      return options.back();
    }

    Option &add_func_option(const std::string &long_name, const std::string &short_name,
                       const std::function<void()> &func)
    {
      options.emplace_back(Option(long_name, short_name, func));
      long_name_index[long_name] = options.size() - 1;
      if (!short_name.empty())
        short_name_index[short_name] = options.size() - 1;
      return options.back();
    }

    Option &add_help(const std::string &long_name, const std::string &short_name)
    {
      auto help = [this] {
        fmt::println("usage: {} [option] ...", name);
        if (!description.empty())
          fmt::println(description);
        fmt::println("options:");
        for (auto &r: long_name_index)
        {
          //TODO improve '__self' information
          if(r.first == "__self") continue ;
          auto &opt = options[r.second];
          if (!opt.short_name.empty())
            fmt::print(" -{}, --{} ", opt.short_name, opt.long_name);
          else
            fmt::print("     --{} ", opt.long_name);

          if (opt.expected_type != index_of_v<bool, ArgsTypeList> && opt.expected_type != 0)
          {
            fmt::print("[{}", get_typename(opt.expected_type));

            if (opt.expected_type != index_of_v<bool, ArgsTypeList> && opt.default_value.index() == opt.expected_type)
              fmt::print(" = {}", to_str(opt.default_value));

            auto restrictor = get_restrictor_description(opt.restrictors);
            if (restrictor.empty())
              fmt::print("]  ");
            else
              fmt::print(", {}]  ", restrictor);
          }
          if (!opt.description.empty())
            fmt::print("  {}", opt.description);
          fmt::print("\n");
        }
      };
      options.emplace_back(Option(long_name, short_name, help));
      long_name_index[long_name] = options.size() - 1;
      if (!short_name.empty())
        short_name_index[short_name] = options.size() - 1;
      return options.back();
    }

    ArgsParser &add_description(const std::string &description_)
    {
      description = description_;
      return *this;
    }

    ArgsParser &set_name(const std::string &name_)
    {
      name = name_;
      return *this;
    }

    ArgsParser &parse(const std::vector<std::string> &args)
    {
      if (args.size() == 0)
        return *this;
      if (name.empty())
        name = args[0];
      Option *curr = nullptr;
      if (auto it = long_name_index.find("__self"); it != long_name_index.end())
        curr = &options[it->second];
      for (size_t i = 1; i < args.size(); i++)
      {
        if (args[i].size() > 1 && args[i][0] == '-')
        {
          if (args[i].size() > 2 && args[i][1] == '-')
          {
            auto lname = args[i].substr(2);
            auto it = long_name_index.find(lname);
            if (it == long_name_index.end())
            {
              fmt::println(stderr, "Warning: Unrecognized option '--{}'.", lname);
              curr = nullptr;
            }
            else
            {
              curr = &options[it->second];
            }
          }
          else
          {
            auto sname = args[i].substr(1);
            auto it = short_name_index.find(sname);
            if (it == short_name_index.end())
            {
              fmt::println(stderr, "Warning: Unrecognized option '-{}'.", sname);
              curr = nullptr;
            }
            else
            {
              curr = &options[it->second];
            }
          }
          if (curr != nullptr && curr->func)
          {
            curr->func();
            curr = nullptr;
            continue;
          }
        }
        else
        {
          if (curr == nullptr)
          {
            fmt::println(stderr, "Warning: Ignored '{}'.", args[i]);
            continue;
          }
          else
          {
            switch (curr->expected_type)
            {
              case index_of_v<bool, ArgsTypeList>:
                parse_value_helper<bool>(curr, args[i]);
                break;
              case index_of_v<int, ArgsTypeList>:
                parse_value_helper<int>(curr, args[i]);
                break;
              case index_of_v<double, ArgsTypeList>:
                parse_value_helper<double>(curr, args[i]);
                break;
              case index_of_v<std::string, ArgsTypeList>:
                parse_value_helper<std::string>(curr, args[i]);
                break;
            }
          }
        }
      }
      return *this;
    }

    template<typename T>
    bool has(const std::string &name)
    {
      auto opt = match(name);
      if (opt == nullptr)
        return false;
      return opt->has<T>();
    }

    template<typename T>
    std::optional<T> get(const std::string &name)
    {
      auto opt = match(name);
      if (opt == nullptr)
        return std::nullopt;
      return opt->get<T>();
    }

  private:
    Option *match(const std::string &name)
    {
      Option *opt = nullptr;
      auto its = short_name_index.find(name);
      auto itl = long_name_index.find(name);
      if (name.size() == 1 && its != short_name_index.end())
        opt = &options[its->second];
      else if (name.size() > 1 && itl != long_name_index.end())
        opt = &options[itl->second];
      else if (itl != long_name_index.end())
        opt = &options[itl->second];
      else if (its != short_name_index.end())
        opt = &options[its->second];
      return opt;
    }
    template<typename T>
    void parse_value_helper(Option *curr, const std::string &s)
    {
      auto opt = str_to<T>(s);
      if (opt.has_value())
      {
        bool pass = true;
        for (auto &restrictorv: curr->restrictors)
        {
          auto restrictor = std::get<Restrictor<T>>(restrictorv);
          if (!restrictor.is_valid(opt.value()))
          {
            fmt::print(stderr, "Warning: Ignored invalid '{}'.", s);
            if (!restrictor.description.empty())
              fmt::print(stderr, "(Restriction: {})\n", restrictor.description);
            else
              fmt::print(stderr, "\n");
            pass = false;
          }
        }
        if (pass)
          curr->value = opt.value();
      }
      else
        fmt::println(stderr, "Warning: Ignored '{}'.", s);
    }
  };
}
#endif