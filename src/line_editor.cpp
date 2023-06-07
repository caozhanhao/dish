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

#include "dish/lexer.hpp"
#include "dish/line_editor.hpp"
#include "dish/utils.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>
#include <tuple>
#include <thread>

#include <sys/ioctl.h>

namespace dish::line_editor
{
  LineEditorContext dle_context;
  enum class SpecialKey
  {
    CTRL_A = 1,
    CTRL_B = 2,
    CTRL_C = 3,
    CTRL_D = 4,
    CTRL_E = 5,
    CTRL_F = 6,

    CTRL_H = 8,
    TAB = 9,
    LINE_FEED = 10,
    CTRL_K = 11,
    CTRL_L = 12,
    ENTER = 13,
    CTRL_N = 14,

    CTRL_P = 16,

    CTRL_T = 20,
    CTRL_U = 21,

    CTRL_W = 23,

    ESC = 27,

    BACKSPACE = 127
  };

  bool is_special_key(int c)
  {
    return (c >= 0 && c <= 6) || (c >= 8 && c <= 14) || c == 16 || c == 20 || c == 21 || c == 23 || c == 27 || c == 127;
  }

  void dle_write(const tiny_utf8::string& str)
  {
    std::cout.write(str.c_str(), str.size());
    std::cout.flush();
    //std::this_thread::sleep_for(std::chrono::milliseconds (500));
  }

  template<typename ...Args>
  void dle_write(const tiny_utf8::string& fmt, Args&& ...args)
  {
    dle_write(fmt::format(fmt.cpp_str(), std::forward<Args>(args)...));
  }

  void dle_init()
  {
    dle_context.pos = 1;
    dle_context.history_pos = 0;
    dle_context.last_refresh_pos = 0;
    dle_context.completion_pos_line = 0;
    dle_context.completion_pos_column = 0;
    dle_context.completion_show_line_pos = 0;
    dle_context.completion_show_line_size = 0;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    dle_context.screen_width = w.ws_col;
    dle_context.screen_height = w.ws_row;
    if (dle_context.screen_width == 0)
    {
      fmt::println(stderr, "Dish Line Editor Error: "
                           "Can not get the screen's width, assuming 128.");
      dle_context.screen_width = 128;
    }
    if (dle_context.screen_height == 0)
    {
      fmt::println(stderr, "Dish Line Editor Error: "
                           "Can not get the screen's width, assuming 10.");
      dle_context.screen_height = 10;
    }
    dle_context.searching_completion = false;
  }

  tiny_utf8::string get_color(int c)
  {
    return fmt::format("\033[{}m", c);
  }

  tiny_utf8::string highlight_line()
  {
    int cmd_color = dish_context.lua_state["dish"]["style"]["cmd"];
    int arg_color = dish_context.lua_state["dish"]["style"]["arg"];
    int str_color = dish_context.lua_state["dish"]["style"]["string"];
    int env_color = dish_context.lua_state["dish"]["style"]["env"];
    int err_color = dish_context.lua_state["dish"]["style"]["error"];

    if (dle_context.line.empty()) return "";
    tiny_utf8::string ret;

    auto it = dle_context.line.raw_cbegin();
    for(;it < dle_context.line.raw_cend() && *it != ' '; ++it);
    auto cmd = dle_context.line.substr(0, it - dle_context.line.raw_cbegin());

    auto [type, cmd_path] = utils::find_command(cmd);
    switch (type)
    {
      case utils::CommandType::not_found:
      case utils::CommandType::not_executable:
        ret += get_color(err_color) + cmd + get_color(0);
        break;
      case utils::CommandType::builtin:
      case utils::CommandType::lua_func:
      case utils::CommandType::executable_file:
      case utils::CommandType::executable_link:
        ret += get_color(cmd_color) + cmd + get_color(0);
        break;
    }

    for (; it < dle_context.line.raw_cend(); ++it)
    {
      switch (*it)
      {
        case ' ':
          ret += get_color(arg_color);
          break;
        case '$':
          ret += get_color(env_color);
          break;
        case '"':
          ret += get_color(str_color);
          break;
      }
      ret += *it;
    }
    if (!dle_context.searching_history_pattern.empty())
    {
      auto beg = ret.find(dle_context.searching_history_pattern);
      if (beg != tiny_utf8::string::npos)
      {
        ret.insert(beg, "\033[48;5;7m");
        ret.insert(beg + 9 + dle_context.searching_history_pattern.length(), "\033[49m");
      }
    }
    ret += get_color(0);
    return ret;
  }

  int save_history(const tiny_utf8::string &path)
  {
    std::ofstream fs(path.cpp_str());
    if (!fs.good())
    {
      fmt::println(stderr, "Failed to save history. (path: '{}').", path);
      return -1;
    }
    for (auto &r: dle_context.history)
      fs << r.timestamp << ";" << r.cmd << "\n";
    fs.close();
    return 0;
  }

  int load_history(const tiny_utf8::string &path)
  {
    std::ifstream fs(path.cpp_str());
    if (!fs.good())
    {
      fmt::println(stderr, "Failed to load history. (path: '{}').", path);
      return -1;
    }
    std::string tmp;
    while (std::getline(fs, tmp))
    {
      if (tmp.find(';') == tiny_utf8::string::npos)
      {
        fmt::println(stderr, "Unknown history format. (path: '{}').", path);
        return -1;
      }
      auto v = utils::split<std::string_view , std::vector<std::string>>(tmp, ";");
      dle_context.history.emplace_back(History{v[1], v[0]});
    }
    fs.close();
    return 0;
  }

  tiny_utf8::string get_hint()
  {
    tiny_utf8::string before_pattern;
    tiny_utf8::string pattern;
    if (dle_context.line.back() != ' ')
    {
      auto i = dle_context.line.rfind(' ', dle_context.pos);
      if (i != tiny_utf8::string::npos && i + 1 < dle_context.line.size())
      {
        pattern = dle_context.line.substr(i + 1);
        before_pattern = dle_context.line.substr(0, i);
      }
    }
    else
    {
      before_pattern = dle_context.line;
      if(!before_pattern.empty()) before_pattern.pop_back();
    }

    // lua hint
    if (sol::protected_function h = dish_context.lua_state["dish"]["hint"]; h.valid())
    {
      auto res = h(before_pattern.cpp_str(), pattern.cpp_str());
      if (res.valid() && res.get_type() == sol::type::string)
      {
        auto r = res.get<std::string>();
        if (!r.empty()) return r;
      }
    }
    // history hint
    auto it = std::find_if(dle_context.history.crbegin(), dle_context.history.crend(),
                           [](auto &&f) { return utils::begin_with(f.cmd, dle_context.line); });
    if (it != dle_context.history.crend())
      return it->cmd.substr(dle_context.line.length());
    // command hint
    if (dle_context.line.find(' ') == tiny_utf8::string::npos)
    {
      auto cmds = utils::match_command(dle_context.line);
      if (!cmds.empty())
        return cmds.begin()->name.substr(dle_context.line.length());
    }
    // file hint
    if(auto files = utils::match_files_and_dirs(pattern); !files.empty())
    {
      if (utils::begin_with(files[0], pattern))
        return files[0].substr(pattern.size());
      else
        return files[0];
    }
    return "";
  }
  void complete_refresh();
  void cmdline_refresh(bool with_hints)
  {
    // move to begin and clear the line
    if (dle_context.last_refresh_pos != 0)
      dle_write("\x1b[{}D\x1b[K", dle_context.last_refresh_pos);
    else
      dle_write("\x1b[K");
    // the current line
    dle_write(highlight_line());
    // hint
    if (with_hints)
    {
      dle_context.hint = get_hint();
      dle_write(utils::effect(dle_context.hint,
                           static_cast<utils::Effect>(dish_context.lua_state["dish"]["style"]["hint"].get<int>())));
    }
    else
      dle_context.hint.clear();
    // move cursor back
    if (dle_context.pos < dle_context.line.size() + dle_context.hint.size())
    {
      dle_write("\x1b[{}D",
                         dle_context.hint.size() + dle_context.line.size() - dle_context.pos);
    }
    // save pos for next refresh
    dle_context.last_refresh_pos = dle_context.pos;
  }
  void edit_refresh_line(bool with_hints)
  {
    if (dle_context.completion.empty())
      cmdline_refresh(with_hints);
    else
      complete_refresh();
  }

  // output, info, raw
  std::vector<std::tuple<tiny_utf8::string, tiny_utf8::string, tiny_utf8::string>>
  get_argument_candidate()
  {
    utils::Effect info_color = static_cast<utils::Effect>
            (dish_context.lua_state["dish"]["style"]["info"].get<int>());
    std::vector<std::tuple<tiny_utf8::string, tiny_utf8::string, tiny_utf8::string>> ret;
    // lua complete
    if (sol::protected_function h = dish_context.lua_state["dish"]["complete"]; h.valid())
    {

      tiny_utf8::string before_pattern;
      auto it = dle_context.line.raw_crbegin();
      for(; it < dle_context.line.raw_crend() && *it != ' '; ++it);
      if(it > dle_context.line.raw_crbegin())
        before_pattern = dle_context.line.substr(0, it - dle_context.line.raw_crbegin());

      auto res = h(before_pattern.cpp_str(), dle_context.complete_pattern.cpp_str());
      if (res.valid() && res.get_type() == sol::type::table)
      {
        for (auto &r: res.get<sol::table>())
        {
          if (r.second.get_type() != sol::type::table)
            break;
          if(auto lhs = r.second.as<sol::table>()[1], rhs = r.second.as<sol::table>()[2];
              lhs.valid() && rhs.valid()
              && lhs.get_type() == sol::type::string && rhs.get_type() == sol::type::string)
          {
            tiny_utf8::string raw = lhs.get<std::string>();
            if(auto raw_lua = r.second.as<sol::table>()[3];
                raw_lua.valid() && raw_lua.get_type() == sol::type::string)
            {
              raw = raw_lua.get<std::string>();
            }
            ret.emplace_back(
                    lhs.get<std::string>(),// output
                    fmt::format("({})", utils::effect(rhs.get<std::string>(),
                            info_color)),// info
                    raw // raw
                    );
          }
          else
          {
            break;
          }
        }
      }
    }
    if (ret.empty())
    {
      auto f = utils::match_files_and_dirs(dle_context.complete_pattern);
      for (auto &r: f)
      {
        tiny_utf8::string raw;
        if(dle_context.complete_pattern.back() == '/')
          raw = dle_context.complete_pattern + r;
        else
          raw = r;
        ret.emplace_back(r,   // output
                         "",  //info
                         raw);// raw
      }
      dle_context.complete_pattern = "";
    }
    return ret;
  }

  // output, info, raw
  std::vector<std::tuple<tiny_utf8::string, tiny_utf8::string, tiny_utf8::string>>
  get_cmd_candidate()
  {
    utils::Effect info_color = static_cast<utils::Effect>
            (dish_context.lua_state["dish"]["style"]["info"].get<int>());
    std::vector<std::tuple<tiny_utf8::string, tiny_utf8::string, tiny_utf8::string>>  ret;
    auto cmds = utils::match_command(dle_context.complete_pattern);
    for (auto &r: cmds)
    {
      tiny_utf8::string info;
      if (r.type == utils::CommandType::executable_file || r.type == utils::CommandType::executable_link)
      {
        info = fmt::format("({}, {})",
                             utils::effect(utils::to_string(r.type), info_color),
                             utils::effect(utils::get_human_readable_size(r.file_size), info_color));
      }
      else
      {
        info = fmt::format("({})", utils::effect(utils::to_string(r.type), info_color));
      }
      ret.emplace_back(r.name, info, r.name);
    }
    return ret;
  }

  // column, line
  std::tuple<size_t, size_t> complete_init(size_t total, size_t max_size)
  {
    dle_context.completion.clear();
    dle_context.completion_show_line_pos = 0;
    dle_context.completion_pos_line = 0;
    dle_context.completion_pos_column = 0;
    size_t columns = dle_context.screen_width / (max_size + 2);
    size_t lines = std::ceil(static_cast<double>(total) / static_cast<double>(columns));
    columns -= (lines * columns - total) / lines;

    dle_context.completion.insert(dle_context.completion.end(), columns, {});
    for (auto &r: dle_context.completion) r.insert(r.end(), lines, {"", ""});
    if(lines > dle_context.screen_height - 3)
      dle_context.completion_show_line_size = dle_context.screen_height - 3;
    else
      dle_context.completion_show_line_size = lines;
    return {columns, lines};
  }

  void complete_select();
  void complete_clear();

  int complete_line()
  {
    // output, info, raw
    std::vector<std::tuple<tiny_utf8::string, tiny_utf8::string, tiny_utf8::string>> items;
    // command
    if (auto it = std::find(dle_context.line.begin(),
                            dle_context.line.begin() + dle_context.pos, ' ');
        it == dle_context.line.end())
    {
      dle_context.complete_pattern = dle_context.line.substr(0, dle_context.pos);
      items = get_cmd_candidate();
    }
    else
    {
      tiny_utf8::string last_word;
      if (dle_context.line.back() != ' ')
      {
        auto i = dle_context.line.rfind(' ', dle_context.pos);
        if (i != tiny_utf8::string::npos && i + 1 < dle_context.line.size())
          last_word = dle_context.line.substr(i + 1);
      }
      dle_context.complete_pattern = last_word;
      items = get_argument_candidate();
    }
    if (items.empty()) return 0;
    else if(items.size() == 1)
    {
      dle_context.completion = {{{"", std::get<0>(items[0])}}};
      complete_select();
      complete_clear();
      return 0;
    }
    auto max_size_item = std::max_element(items.cbegin(), items.cend(),
                                          [](auto &&a, auto &&b) {
                                            return (utils::display_length(std::get<0>(a)) + utils::display_length(std::get<1>(a))<
                                                    utils::display_length(std::get<0>(b)) + utils::display_length(std::get<1>(b)));
                                          });
    size_t max_size = utils::display_length(std::get<0>(*max_size_item)) + utils::display_length(std::get<1>(*max_size_item));
    auto [columns, lines] = complete_init(items.size(), max_size);
    size_t line = 0;
    size_t column = 0;
    for (auto &r: items)
    {
      auto [out, info, raw] = r;
      tiny_utf8::string output = out;
      output.insert(output.end(), tiny_utf8::string(max_size - out.length() - utils::display_size(info), ' '));
      output += info;
      dle_context.completion[column][line] = {output, raw};
      if (line < lines - 1)
        ++line;
      else
      {
        ++column;
        line = 0;
      }
    }
    return 0;
  }

  // bool from_complete_move -> avoid recursion
  void complete_up(bool);
  void complete_down(bool);
  void complete_right(bool);
  void complete_left(bool);
  void complete_left(bool from_complete_move = false)
  {
    if (dle_context.completion_pos_column > 0)
      --dle_context.completion_pos_column;
    else
    {
      dle_context.completion_pos_column = dle_context.completion.size() - 1;
      if(!from_complete_move)
        complete_up(true);
    }
    complete_refresh();
    if(auto& [c, r] = dle_context.completion[dle_context.completion_pos_column][dle_context.completion_pos_line];
        c.empty())
      complete_left();
  }
  void complete_right(bool from_complete_move = false)
  {
    if (dle_context.completion_pos_column < dle_context.completion.size() - 1)
      ++dle_context.completion_pos_column;
    else
    {
      dle_context.completion_pos_column = 0;
      if(!from_complete_move)
        complete_down(true);
    }
    if(auto& [c, r] = dle_context.completion[dle_context.completion_pos_column][dle_context.completion_pos_line];
        c.empty())
      complete_right();
  }

  void complete_down(bool from_complete_move = false)
  {
    if (dle_context.completion_pos_line < dle_context.completion[0].size() - 1)
      ++dle_context.completion_pos_line;
    else
    {
      dle_context.completion_pos_line = 0;
      dle_context.completion_show_line_pos = 0;
      if(!from_complete_move)
        complete_right(true);
    }
    if(dle_context.completion_pos_line == dle_context.completion_show_line_pos + dle_context.completion_show_line_size)
    {
      if(dle_context.completion_show_line_pos <
          dle_context.completion[0].size() - dle_context.completion_show_line_size)
        ++dle_context.completion_show_line_pos;
    }
    if(auto& [c, r] = dle_context.completion[dle_context.completion_pos_column][dle_context.completion_pos_line];
        c.empty())
      complete_down();
  }

  void complete_up(bool from_complete_move = false)
  {
    if (dle_context.completion_pos_line > 0)
      --dle_context.completion_pos_line;
    else
    {
      dle_context.completion_pos_line = dle_context.completion[0].size() - 1;
      dle_context.completion_show_line_pos =
              dle_context.completion[0].size() - dle_context.completion_show_line_size;
      if(!from_complete_move)
        complete_left(true);
    }
    if(dle_context.completion_pos_line + 1 == dle_context.completion_show_line_pos)
    {
      if(dle_context.completion_show_line_pos > 0)
        --dle_context.completion_show_line_pos;
    }
    if(auto& [c, r] = dle_context.completion[dle_context.completion_pos_column][dle_context.completion_pos_line];
        c.empty())
      complete_up();
  }

  int get_columns_before_complete()
  {
    size_t ret = 0;
    auto i = dle_context.prompt.rfind('\n');
    auto j = dle_context.prompt.rfind('\r');
    if (i != tiny_utf8::string::npos || j != tiny_utf8::string::npos)
    {
      size_t last_line;
      if (i == tiny_utf8::string::npos && j != tiny_utf8::string::npos)
        last_line = j;
      else if (i != tiny_utf8::string::npos && j == tiny_utf8::string::npos)
        last_line = i;
      else if (i != tiny_utf8::string::npos && j != tiny_utf8::string::npos)
        last_line = std::min(i, j);
      ret += utils::display_size(dle_context.prompt.substr(last_line + 1));
    }
    ret += dle_context.last_refresh_pos;
    return ret;
  }

  void complete_apply()
  {
    const auto &cmd = std::get<1>(dle_context.completion[dle_context.completion_pos_column]
                                                        [dle_context.completion_pos_line]);
    auto b = dle_context.pos - 1;
    auto e = dle_context.pos - 1;
    while (b > 0 && dle_context.line[b] != ' ')
      --b;
    while (e < dle_context.line.size()  && dle_context.line[e] != ' ')
      ++e;
    dle_context.line.erase(b, e - b);
    if (b != 0)
      dle_context.line.insert(b++, ' ');
    dle_context.line.insert(b, cmd);
    dle_context.pos = b + cmd.size();
    dle_context.last_refresh_pos = b + cmd.length();
  }

  void complete_refresh()
  {
    size_t last_size = 0;
    size_t lines = dle_context.completion[0].size();
    size_t screen_lines = std::min(dle_context.completion_show_line_size, dle_context.completion[0].size());
    size_t columns = dle_context.completion.size();
    tiny_utf8::string newline = "\n";
    for (size_t i = 0; i < screen_lines; ++i)
      newline += "\n\x1b[2K";
    dle_write(newline);
    dle_write(utils::effect(fmt::format("lines: {}/{}", dle_context.completion_pos_line + 1, dle_context.completion[0].size()),
                            utils::Effect::bg_cyan));
    dle_write("\x1b[{}F", screen_lines);
    for (size_t c = 0; c < columns; ++c)
    {
      for (size_t l = dle_context.completion_show_line_pos;
           l < lines && l < dle_context.completion_show_line_pos + dle_context.completion_show_line_size; ++l)
      {
        const auto &line = std::get<0>(dle_context.completion[c][l]);
        if(!line.empty())
        {
          tiny_utf8::string out;
          // highlight
          if (dle_context.searching_completion &&
              c == dle_context.completion_pos_column && l == dle_context.completion_pos_line)
          {
            out = utils::effect(dle_context.complete_pattern,
                                utils::Effect::bold, utils::Effect::underline,
                                utils::Effect::bg_strong_shadow);
            out += utils::effect(line.substr(dle_context.complete_pattern.length()),
                                 utils::Effect::bg_strong_shadow);
          }
          else
          {
            out = utils::effect(dle_context.complete_pattern,
                                utils::Effect::bold, utils::Effect::underline);
            if (line != dle_context.complete_pattern)
              out += line.substr(dle_context.complete_pattern.length());
          }
          last_size = utils::display_size(out);
          dle_write("{}\x1b[{}D\x1b[1B", out, last_size);
        }
        else // Padding
          dle_write("\x1b[1D\x1b[1B");
      }
      if (c < columns - 1)
        dle_write("\x1b[{}A\x1b[{}C", screen_lines, last_size + 2);
    }

    if (dle_context.searching_completion)
      complete_apply();
    // move cursor back
    dle_write("\x1b[{}F", screen_lines + 1);
    if (auto i = get_columns_before_complete(); i != 0)
      dle_write("\x1b[{}C", i);
    cmdline_refresh(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void complete_clear()
  {
    size_t screen_lines = 1 + std::min(dle_context.completion_show_line_size, dle_context.completion[0].size());
    for(size_t i = 0; i < screen_lines; ++i)
      dle_write("\x1b[B\x1b[2K");
    dle_write("\x1b[{}F", screen_lines);
    if (auto i = get_columns_before_complete(); i != 0)
      dle_write("\x1b[{}C", i);
    dle_context.completion.clear();
    dle_context.searching_completion = false;
    dle_context.completion_show_line_pos = 0;
    dle_context.completion_show_line_size = 0;
    dle_context.completion_pos_line = 0;
    dle_context.completion_pos_column = 0;
  }

  void complete_select()
  {
    complete_apply();
    complete_clear();
    if(dle_context.line[dle_context.pos - 1] != '/')
    {
      dle_context.line.insert(dle_context.line.begin() + dle_context.pos, ' ');
      ++dle_context.pos;
    }
  }

  void move_to_beginning()
  {
    if (dle_context.pos == 0) return;
    auto origin = dle_context.pos;
    dle_context.pos = 0;
    dle_write("\x1b[{}D", origin);
    dle_context.last_refresh_pos = dle_context.pos;
  }

  void move_to_end()
  {
    if (dle_context.pos == dle_context.line.length() && dle_context.hint.empty()) return;
    bool refresh = false;
    if (!dle_context.hint.empty())
    {
      dle_context.line += dle_context.hint;
      dle_context.hint.clear();
      refresh = true;
    }
    auto origin = dle_context.pos;
    dle_context.pos = dle_context.line.length();
    dle_write("\x1b[{}C", dle_context.pos - origin);
    dle_context.last_refresh_pos = dle_context.pos;
    if (refresh) edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void move_to_word_beginning()
  {
    auto origin = dle_context.pos;
    if (dle_context.line[dle_context.pos - 1] == ' ')
      --dle_context.pos;
    // curr is not space
    while (dle_context.pos > 0 && dle_context.line[dle_context.pos] == ' ')
      --dle_context.pos;
    // prev is space or begin
    while (dle_context.pos > 0 && dle_context.line[dle_context.pos - 1] != ' ')
      --dle_context.pos;
    dle_write("\x1b[{}D", origin - dle_context.pos);
    dle_context.last_refresh_pos = dle_context.pos;
  }

  void move_to_word_end()
  {
    auto origin = dle_context.pos;
    // curr is not space
    while (dle_context.pos < dle_context.line.length() && dle_context.line[dle_context.pos] == ' ')
      ++dle_context.pos;
    // next is space or end
    while (dle_context.pos < dle_context.line.length() && dle_context.line[dle_context.pos] != ' ')
      ++dle_context.pos;
    dle_write("\x1b[{}C", dle_context.pos - origin);
    dle_context.last_refresh_pos = dle_context.pos;
  }

  void move_left()
  {
    if (dle_context.pos > 0)
    {
      auto origin = dle_context.pos;
      dle_context.pos = ((dle_context.line.begin() + dle_context.pos) - 1) - dle_context.line.begin();
      dle_context.last_refresh_pos = dle_context.pos;
      dle_write("\x1b[{}D", origin - dle_context.pos);
    }
  }

  void move_right()
  {
    if (dle_context.pos < dle_context.line.length())
    {
      auto origin = dle_context.pos;
      dle_context.pos = ((dle_context.line.begin() + dle_context.pos) + 1) - dle_context.line.begin();
      dle_context.last_refresh_pos = dle_context.pos;
      dle_write("\x1b[{}C", dle_context.pos - origin);
    }
  }

  void edit_delete()
  {
    if (dle_context.pos >= dle_context.line.length()) return;
    dle_context.line.erase(dle_context.pos, 1);
    edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void edit_backspace()
  {
    if (dle_context.pos == 0) return;
    dle_context.line.erase(dle_context.pos - 1, 1);
    --dle_context.pos;
    edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void edit_delete_next_word()
  {
    if (dle_context.pos == dle_context.line.length() - 1) return;
    auto i = dle_context.pos;
    // skip space
    while (i < dle_context.line.length() && dle_context.line[i] == ' ')
      ++i;
    // find end
    while (i < dle_context.line.length() && dle_context.line[i] != ' ')
      ++i;
    dle_context.line.erase(dle_context.pos + 1, i - dle_context.pos);
    edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void edit_history_helper(bool prev)
  {
    // first search should save the current line
    if (dle_context.searching_history_pattern.empty())
      dle_context.history.back().cmd = dle_context.line;
    auto next_history = [prev]() -> int {
      auto origin = dle_context.history_pos;
      while (dle_context.history[origin].cmd == dle_context.history[dle_context.history_pos].cmd)
      // skip same command
      {
        if (prev)
        {
          if (dle_context.history_pos != 0)
            --dle_context.history_pos;
          else
            return -1;
        }
        else
        {
          if (dle_context.history_pos != dle_context.history.size())
            ++dle_context.history_pos;
          else
            return -1;
        }
      }
      return 0;
    };
    if (!dle_context.line.empty())
    {
      auto origin = dle_context.history_pos;
      bool found = false;
      // If this is the first search, save it for next search and highlight.
      // Otherwise, restore the pattern saved previously
      if (dle_context.searching_history_pattern.empty())
        dle_context.searching_history_pattern = dle_context.line;
      while (next_history() != -1)
      {
        auto f = dle_context.history[dle_context.history_pos].cmd.find(dle_context.searching_history_pattern);
        if (f == tiny_utf8::string::npos)
          continue;
        found = true;
        break;
      }
      if (!found)
        dle_context.history_pos = origin;
    }
    else
      next_history();
    dle_context.line = dle_context.history[dle_context.history_pos].cmd;
    dle_context.pos = dle_context.line.length();
    edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
    move_to_end();
  }

  void edit_up()
  {
    if (dle_context.completion.empty())
      edit_history_helper(true);
    else
      complete_up();
  }

  void edit_down()
  {
    if (dle_context.completion.empty())
      edit_history_helper(false);
    else
      complete_down();
  }

  void edit_left()
  {
    if (dle_context.completion.empty())
      move_left();
    else
      complete_left();
  }
  void edit_right()
  {
    if (dle_context.completion.empty())
      move_right();
    else
      complete_right();
  }

  void clear_screen()
  {
    dle_write("\x1b[H\x1b[2J");
  }

  // the core of Dish Line Editor
  int edit_line()
  {
    dle_context.line.clear();
    dle_context.last_refresh_pos = 0;
    dle_context.pos = 0;
    dle_context.history.emplace_back(History{"", ""});
    dle_context.history_pos = dle_context.history.size() - 1;
    while (true)
    {
      char buf[32];
      std::cin.read(buf, 1);
      if (is_special_key(static_cast<int>(buf[0])))
      {
        SpecialKey key = static_cast<SpecialKey>(buf[0]);
        if (key == SpecialKey::TAB)
        {
          if (dle_context.completion.empty())
          {
            complete_line();
            edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
            dle_context.searching_completion = true;
          }
          else
          {
            edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
            if(dle_context.searching_completion)
              complete_down();
          }
          continue;
        }
        switch (key)
        {
          case SpecialKey::CTRL_A:
            if(!dle_context.completion.empty())
              complete_select();
            move_to_beginning();
            break;
          case SpecialKey::CTRL_B:
            edit_left();
            break;
          case SpecialKey::CTRL_C:
            dle_context.history.pop_back();
            dle_context.line.clear();
            dle_context.pos = 0;
            dish_context.lua_state["last_foreground_ret"] = 127;
            dle_write("\033[40;37m^C\033[0m");
            if(!dle_context.completion.empty())
            {
              dle_write("\x1b[{}E", dle_context.completion[0].size());
              dle_context.searching_completion = false;
              dle_context.completion.clear();
            }
            return 0;
            break;
          case SpecialKey::CTRL_D:
            edit_delete();
            break;
          case SpecialKey::CTRL_E:
            if(!dle_context.completion.empty())
              complete_select();
            move_to_end();
            break;
          case SpecialKey::CTRL_F:
            edit_right();
            break;
          case SpecialKey::CTRL_K:
            if(!dle_context.completion.empty())
              complete_select();
            dle_context.line.erase(dle_context.pos);
            break;
          case SpecialKey::CTRL_L:
            if(!dle_context.completion.empty())
              complete_select();
            clear_screen();
            break;
          case SpecialKey::LINE_FEED:
          case SpecialKey::ENTER:
            if(!dle_context.completion.empty())
            {
              complete_select();
              edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
            }
            else
            {
              if (dle_context.line.empty())
                dle_context.history.pop_back();
              else
              {
                dle_context.history.back().cmd = dle_context.line;
                dle_context.history.back().timestamp = utils::get_timestamp();
              }
              edit_refresh_line(false);
              return 0;
            }
            break;
          case SpecialKey::CTRL_N:
            edit_down();
            break;
          case SpecialKey::CTRL_P:
            edit_up();
            break;
          case SpecialKey::CTRL_T:
            if(!dle_context.completion.empty())
              complete_select();
            if (dle_context.pos != 0)
            {
              auto tmp = dle_context.line[dle_context.pos];
              dle_context.line[dle_context.pos] = dle_context.line[dle_context.pos - 1];
              dle_context.line[dle_context.pos - 1] = tmp;
            }
            break;
          case SpecialKey::CTRL_U:
            if(!dle_context.completion.empty())
              complete_select();
            dle_context.line.clear();
            dle_context.pos = 0;
            break;
          case SpecialKey::CTRL_W:
          {
            if(!dle_context.completion.empty())
              complete_select();
            if (dle_context.pos == 0) break;
            auto origin_pos = dle_context.pos;
            while (dle_context.pos > 0 && dle_context.line[dle_context.pos - 1] == ' ')
              dle_context.pos--;
            while (dle_context.pos > 0 && dle_context.line[dle_context.pos - 1] != ' ')
              dle_context.pos--;
            dle_context.line.erase(dle_context.pos, origin_pos - dle_context.pos);
            break;
          }
          case SpecialKey::ESC:// Escape Sequence
            char seq[3];
            std::cin.read(seq, 1);
            // esc ?
            if (seq[0] != '[' && seq[0] != 'O')
            {
              switch (seq[0])
              {
                case 'd':
                  edit_delete_next_word();
                  break;
                case 'b':
                  move_to_word_beginning();
                  break;
                case 'f':
                  move_to_word_end();
                  break;
              }
            }
            else
            {
              std::cin.read(seq + 1, 1);
              // esc [
              if (seq[0] == '[')
              {
                if (seq[1] >= '0' && seq[1] <= '9')
                {
                  std::cin.read(seq + 2, 1);
                  if (seq[2] == '~' && seq[1] == '3')
                  {
                    edit_delete();
                  }
                  else if (seq[2] == ';')
                  {
                    std::cin.read(seq, 2);
                    if (seq[0] == '5' && seq[1] == 'C')
                      move_to_word_end();
                    if (seq[0] == '5' && seq[1] == 'D')
                      move_to_word_beginning();
                  }
                }
                else
                {
                  switch (seq[1])
                  {
                    case 'A':
                      edit_up();
                      break;
                    case 'B':
                      edit_down();
                      break;
                    case 'C':
                      edit_right();
                      break;
                    case 'D':
                      edit_left();
                      break;
                    case 'H':
                      move_to_beginning();
                      break;
                    case 'F':
                      move_to_end();
                      break;
                    case 'd':
                      edit_delete_next_word();
                      break;
                    case '1':
                      move_to_beginning();
                      break;
                    case '4':
                      move_to_end();
                      break;
                  }
                }
              }
              // esc 0
              else if (seq[0] == 'O')
              {
                switch (seq[1])
                {
                  case 'A':
                    edit_up();
                    break;
                  case 'B':
                    edit_down();
                    break;
                  case 'C':
                    edit_right();
                    break;
                  case 'D':
                    edit_left();
                    break;
                  case 'H':
                    move_to_beginning();
                    break;
                  case 'F':
                    move_to_end();
                    break;
                }
              }
            }
            break;
          case SpecialKey::BACKSPACE:
          case SpecialKey::CTRL_H:
            if(!dle_context.completion.empty())
              complete_clear();
            edit_backspace();
            break;
          default:
            fmt::println(stderr, "\nWARNING: Ignored unrecognized key '{}'.", buf[0]);
            dle_context.history.pop_back();
            dle_context.line.clear();
            dle_context.pos = 0;
            dish_context.lua_state["last_foreground_ret"] = -1;
            return -1;
            break;
        }
      }
      else
      {
        dle_context.line.insert(dle_context.line.begin() + dle_context.pos, buf[0]);
        // Input a character should close the history searching and competion.
        dle_context.searching_history_pattern.clear();
        if(!dle_context.completion.empty())
          complete_clear();
        dle_context.completion_pos_line = 0;
        dle_context.completion_pos_column = 0;
        ++dle_context.pos;
        edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
      }
    }
    return -1;
  }

  tiny_utf8::string read_line(const tiny_utf8::string &prompt)
  {
    dle_context.prompt = prompt;
    dle_write(dle_context.prompt);
    edit_line();
    dle_write("\n");
    return dle_context.line;
  }
}