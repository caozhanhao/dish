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
#include <tuple>

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

  void dle_init()
  {
    dle_context.pos = 1;
    dle_context.history_pos = 0;
    dle_context.last_refresh_pos = 0;
    dle_context.completion_pos_line = 0;
    dle_context.completion_pos_column = 0;
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    dle_context.screen_width = w.ws_col;
    if (dle_context.screen_width == 0)
    {
      fmt::println(stderr, "Dish Line Editor Error: "
                           "Can not get the screen's width, assuming 128.");
      dle_context.screen_width = 128;
    }
    dle_context.searching_completion = false;
  }

  void dle_write(const std::string &c)
  {
    std::cout.write(c.c_str(), c.size());
    // debug
    std::cout.flush();
  }

  std::string get_color(int c)
  {
    return fmt::format("\033[{}m", c);
  }

  std::string highlight_line()
  {
    int cmd_color = dish_context.lua_state["dish"]["style"]["cmd"];
    int arg_color = dish_context.lua_state["dish"]["style"]["arg"];
    int str_color = dish_context.lua_state["dish"]["style"]["string"];
    int env_color = dish_context.lua_state["dish"]["style"]["env"];
    int err_color = dish_context.lua_state["dish"]["style"]["error"];

    if (dle_context.line.empty()) return "";
    std::string ret;
    size_t i = 0;
    while (i < dle_context.line.size() && dle_context.line[i] != ' ') ++i;
    auto cmd = dle_context.line.substr(0, i);
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

    for (; i < dle_context.line.size(); ++i)
    {
      switch (dle_context.line[i])
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
      ret += dle_context.line[i];
    }
    if (!dle_context.searching_history_pattern.empty())
    {
      auto beg = ret.find(dle_context.searching_history_pattern);
      if (beg != std::string::npos)
      {
        ret.insert(beg, "\033[48;5;7m");
        ret.insert(beg + 9 + dle_context.searching_history_pattern.size(), "\033[49m");
      }
    }
    ret += get_color(0);
    return ret;
  }

  int save_history(const std::string &path)
  {
    std::ofstream fs(path);
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

  int load_history(const std::string &path)
  {
    std::ifstream fs(path);
    if (!fs.good())
    {
      fmt::println(stderr, "Failed to load history. (path: '{}').", path);
      return -1;
    }
    std::string tmp;
    while (std::getline(fs, tmp))
    {
      if (tmp.find(';') == std::string::npos)
      {
        fmt::println(stderr, "Unknown history format. (path: '{}').", path);
        return -1;
      }
      auto v = utils::split<std::vector<std::string>>(tmp, ";");
      dle_context.history.emplace_back(History{v[1], v[0]});
    }
    fs.close();
    return 0;
  }

  std::string get_hint()
  {
    // lua hint
    if (sol::protected_function h = dish_context.lua_state["dish"]["hint"]; h.valid())
    {
      auto res = h(dle_context.line);
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
      return it->cmd.substr(dle_context.line.size());
    // command hint
    if (dle_context.line.find(' ') == std::string::npos)
    {
      auto cmds = utils::match_command(dle_context.line);
      if (!cmds.empty())
        return cmds.begin()->name.substr(dle_context.line.size());
    }
    return "";
  }

  std::vector<std::string> get_complete_items(const std::string& pattern)
  {
    std::vector<std::string> ret;
    // lua complete
    if (sol::protected_function h = dish_context.lua_state["dish"]["complete"]; h.valid())
    {
      auto res = h(dle_context.line);
      if (res.valid() && res.get_type() == sol::type::table)
      {
        for (auto &r: res.get<sol::table>())
        {
          if (r.second.get_type() != sol::type::string)
          {
            fmt::println(stderr, "Unexpected complete from lua.");
            break;
          }
          ret.emplace_back(r.second.as<std::string>());
        }
      }
    }
    if (ret.empty())
      return utils::match_files_and_dirs(pattern);
    return ret;
  }

  void complete_refresh();
  void cmdline_refresh(bool with_hints = true)
  {
    std::string esc;
    // move to begin and clear the line
    if (dle_context.last_refresh_pos != 0)
      esc += fmt::format("\x1b[{}D\x1b[K", dle_context.last_refresh_pos);
    else
      esc += "\x1b[K";
    // the current line
    esc += highlight_line();
    // hint
    if (with_hints)
    {
      dle_context.hint = get_hint();
      esc += utils::effect(dle_context.hint,
                           static_cast<utils::Effect>(dish_context.lua_state["dish"]["style"]["hint"].get<int>()));
    }
    else
      dle_context.hint.clear();
    // move cursor back
    if (dle_context.pos < dle_context.line.size() + dle_context.hint.size())
    {
      esc += fmt::format("\x1b[{}D",
                         dle_context.hint.size() + dle_context.line.size() - dle_context.pos);
    }
    // save pos for next refresh
    dle_context.last_refresh_pos = dle_context.pos;
    dle_write(esc);
  }
  void edit_refresh_line(bool with_hints = true)
  {
    if (dle_context.completion.empty())
      cmdline_refresh(with_hints);
    else
      complete_refresh();
  }

  // color cmd, info, raw cmd
  std::tuple<std::string, std::string, std::string>
  cmd_to_string(const utils::Command &r, const std::string &pattern)
  {
    auto name = utils::effect(pattern,
                              utils::Effect::bold, utils::Effect::underline);
    name += r.name.substr(pattern.size());
    if (r.type == utils::CommandType::executable_file || r.type == utils::CommandType::executable_link)
    {
      return {name, fmt::format("({}, {})", utils::to_string(r.type),
                                utils::get_human_readable_size(r.file_size)), r.name};
    }
    return {name, fmt::format("({})", utils::to_string(r.type)), r.name};
  }

  std::vector<std::tuple<std::string, std::string, std::string>>
  get_cmd_candidate()
  {
    std::vector<std::tuple<std::string, std::string, std::string>> ret;
    auto cmds = utils::match_command(dle_context.line);
    for (auto &r: cmds)
      ret.emplace_back(cmd_to_string(r, dle_context.line));
    return ret;
  }

  // column, line
  std::tuple<size_t, size_t> init_completion_vec(size_t total, size_t max_size)
  {
    size_t columns = dle_context.screen_width / (max_size + 2);
    size_t lines = std::ceil(static_cast<double>(total) / static_cast<double>(columns));
    columns -= (lines * columns - total) / lines;

    dle_context.completion.insert(dle_context.completion.end(), columns, {});
    for (auto &r: dle_context.completion) r.insert(r.end(), lines, {"", ""});
    return {columns, lines};
  }

  int complete_line()
  {
    dle_context.completion.clear();
    dle_context.completion_pos_line = 0;
    dle_context.completion_pos_column = 0;
    // command
    if (dle_context.line.find(' ') == std::string::npos)
    {
      auto cmds = get_cmd_candidate();
      if (cmds.empty()) return 0;
      auto max_size_item = std::max_element(cmds.cbegin(), cmds.cend(),
                                            [](auto &&a, auto &&b) {
                                              return (std::get<0>(a).size() + std::get<1>(a).size() <
                                                      std::get<0>(b).size() + std::get<1>(b).size());
                                            });
      size_t max_size = std::get<0>(*max_size_item).size() + std::get<1>(*max_size_item).size() + 1;
      auto [columns, lines] = init_completion_vec(cmds.size(), max_size);
      size_t line = 0;
      size_t column = 0;
      for (auto &r: cmds)
      {
        auto [cmd, info, raw_cmd] = r;
        std::string output = cmd;
        output.insert(output.end(), max_size - cmd.size() - info.size(), ' ');
        output += info;
        dle_context.completion[column][line] = {std::move(output), raw_cmd};
        if (line < lines - 1)
          ++line;
        else
        {
          ++column;
          line = 0;
        }
      }
    }
    else
    {
      std::string last_word;
      if (dle_context.line.back() != ' ')
      {
        auto i = dle_context.line.rfind(' ');
        if (i != std::string::npos && i + 1 < dle_context.line.size())
          last_word = dle_context.line.substr(i + 1);
      }
      // from lua
      auto items = get_complete_items(last_word);
      if (items.empty()) return 0;
      auto max_size = std::max_element(items.cbegin(), items.cend(),
                                       [](auto &&a, auto &&b) {
                                         return a.size() < b.size();
                                       })
                              ->size();
      auto [columns, lines] = init_completion_vec(items.size(), max_size);
      size_t line = 0;
      size_t column = 0;
      for (auto &r: items)
      {
        std::string output = utils::effect(last_word,
                                           utils::Effect::bold, utils::Effect::underline);
        output += r.substr(last_word.size());
        output.insert(output.end(), max_size - r.size(), ' ');
        dle_context.completion[column][line] = {output, r};
        if (line < lines - 1)
          ++line;
        else
        {
          ++column;
          line = 0;
        }
      }
    }
    return 0;
  }

  void complete_left()
  {
    if (dle_context.completion_pos_column > 0)
      --dle_context.completion_pos_column;
    else
      dle_context.completion_pos_column = dle_context.completion.size() - 1;
    complete_refresh();
  }
  void complete_right()
  {
    if (dle_context.completion_pos_column < dle_context.completion.size() - 1)
      ++dle_context.completion_pos_column;
    else
      dle_context.completion_pos_column = 0;
    complete_refresh();
  }

  void complete_down()
  {
    if (dle_context.completion_pos_line < dle_context.completion[0].size() - 1)
      ++dle_context.completion_pos_line;
    else
      dle_context.completion_pos_line = 0;
    complete_refresh();
  }

  void complete_up()
  {
    if (dle_context.completion_pos_line > 0)
      --dle_context.completion_pos_line;
    else
      dle_context.completion_pos_line = dle_context.completion[0].size() - 1;
    complete_refresh();
  }

  int get_columns_before_complete()
  {
    size_t ret = 0;
    auto i = dle_context.prompt.rfind('\n');
    auto j = dle_context.prompt.rfind('\r');
    if (i != std::string::npos || j != std::string::npos)
    {
      size_t last_line;
      if (i == std::string::npos && j != std::string::npos)
        last_line = j;
      else if (i != std::string::npos && j == std::string::npos)
        last_line = i;
      else if (i != std::string::npos && j != std::string::npos)
        last_line = std::min(i, j);
      ret += utils::get_length_without_ansi_escape(dle_context.prompt.substr(last_line + 1));
    }
    ret += dle_context.last_refresh_pos;
    return ret;
  }

  void complete_apply()
  {
    auto cmd = std::get<1>(dle_context.completion[dle_context.completion_pos_column]
                                                 [dle_context.completion_pos_line]);
    auto b = dle_context.pos;
    auto e = dle_context.pos;
    while (b > 0 && dle_context.line[b] != ' ')
      --b;
    while (e < dle_context.line.size() && dle_context.line[e] != ' ')
      ++e;
    dle_context.line.erase(b, e - b);
    if(b != 0)
      dle_context.line.insert(b++, 1, ' ');
    dle_context.line.insert(b, cmd);
    dle_context.pos = b + cmd.size();
    dle_context.last_refresh_pos = dle_context.pos;
  }

  void complete_refresh()
  {
    size_t size = utils::get_length_without_ansi_escape(std::get<0>(dle_context.completion[0][0]));
    size_t lines = dle_context.completion[0].size();
    size_t columns = dle_context.completion.size();
    std::string esc = "\n";
    for (size_t i = 0; i < lines; ++i)
      esc += "\n\x1b[2K";
    dle_write(fmt::format("{}\x1b[{}F", esc, lines));
    for (size_t c = 0; c < columns; ++c)
    {
      for (size_t l = 0; l < lines; ++l)
      {
        const auto &line = dle_context.completion[c][l];
        std::string output;
        if (dle_context.searching_completion &&
            c == dle_context.completion_pos_column && l == dle_context.completion_pos_line)
        {
          complete_apply();
          output = "\033[48;5;8m" + std::get<0>(line) + "\033[49m";
        }
        else
          output = std::get<0>(line);
        dle_write(fmt::format("{}\x1b[{}D\x1b[1B", output, utils::get_length_without_ansi_escape(output)));
      }
      if (c < columns - 1)
        dle_write(fmt::format("\x1b[{}A\x1b[{}C", lines, size + 2));
    }

    // move cursor back
    dle_write(fmt::format("\x1b[{}F", lines + 1));
    if (auto i = get_columns_before_complete(); i != 0)
      dle_write(fmt::format("\x1b[{}C", i));
    cmdline_refresh();
  }

  void complete_clear()
  {
    for(size_t i = 0; i < dle_context.completion[0].size(); ++i)
      dle_write("\x1b[B\x1b[2K");
    dle_write(fmt::format("\x1b[{}F", dle_context.completion[0].size()));
    if (auto i = get_columns_before_complete(); i != 0)
      dle_write(fmt::format("\x1b[{}C", i));
    dle_context.completion.clear();
    dle_context.searching_completion = false;
  }

  void move_to_beginning()
  {
    if (dle_context.pos == 0) return;
    auto origin = dle_context.pos;
    dle_context.pos = 0;
    auto esc = fmt::format("\x1b[{}D", origin);
    dle_context.last_refresh_pos = dle_context.pos;
    dle_write(esc);
  }

  void move_to_end()
  {
    if (dle_context.pos == dle_context.line.size() && dle_context.hint.empty()) return;
    bool refresh = false;
    if (!dle_context.hint.empty())
    {
      dle_context.line += dle_context.hint;
      dle_context.hint.clear();
      refresh = true;
    }
    auto origin = dle_context.pos;
    dle_context.pos = dle_context.line.size();
    auto esc = fmt::format("\x1b[{}C", dle_context.pos - origin);
    dle_context.last_refresh_pos = dle_context.pos;
    dle_write(esc);
    if (refresh) edit_refresh_line();
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
    auto esc = fmt::format("\x1b[{}D", origin - dle_context.pos);
    dle_context.last_refresh_pos = dle_context.pos;
    dle_write(esc);
  }

  void move_to_word_end()
  {
    auto origin = dle_context.pos;
    // curr is not space
    while (dle_context.pos < dle_context.line.size() && dle_context.line[dle_context.pos] == ' ')
      ++dle_context.pos;
    // next is space or end
    while (dle_context.pos < dle_context.line.size() && dle_context.line[dle_context.pos] != ' ')
      ++dle_context.pos;
    auto esc = fmt::format("\x1b[{}C", dle_context.pos - origin);
    dle_context.last_refresh_pos = dle_context.pos;
    dle_write(esc);
  }

  void move_left()
  {
    if (dle_context.pos > 0)
    {
      --dle_context.pos;
      dle_context.last_refresh_pos = dle_context.pos;
      dle_write("\x1b[D");
    }
  }

  void move_right()
  {
    if (dle_context.pos < dle_context.line.size())
    {
      ++dle_context.pos;
      dle_context.last_refresh_pos = dle_context.pos;
      dle_write("\x1b[C");
    }
  }

  void edit_delete()
  {
    if (dle_context.pos >= dle_context.line.size()) return;
    dle_context.line.erase(dle_context.pos, 1);
    edit_refresh_line();
  }

  void edit_backspace()
  {
    if (dle_context.pos == 0) return;
    dle_context.line.erase(dle_context.pos - 1, 1);
    --dle_context.pos;
    edit_refresh_line();
  }

  void edit_delete_next_word()
  {
    if (dle_context.pos == dle_context.line.size() - 1) return;
    auto i = dle_context.pos;
    // skip space
    while (i < dle_context.line.size() && dle_context.line[i] == ' ')
      ++i;
    // find end
    while (i < dle_context.line.size() && dle_context.line[i] != ' ')
      ++i;
    dle_context.line.erase(dle_context.pos + 1, i - dle_context.pos);
    edit_refresh_line();
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
        if (f == std::string::npos)
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
    dle_context.pos = dle_context.line.size();
    edit_refresh_line();
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
            edit_refresh_line();
          }
          else
          {
            if(dle_context.searching_completion)
            {
              if(dle_context.completion_pos_column != dle_context.completion.size() - 1)
                complete_right();
              else if(dle_context.completion_pos_line != dle_context.completion[0].size() - 1)
              {
                dle_context.completion_pos_column = 0;
                ++dle_context.completion_pos_line;
                complete_refresh();
              }
              else
              {
                dle_context.completion_pos_column = 0;
                dle_context.completion_pos_line = 0;
                complete_refresh();
              }
            }
            dle_context.searching_completion = true;
            edit_refresh_line();
          }
          continue;
        }
        switch (key)
        {
          case SpecialKey::CTRL_A:
            if(!dle_context.completion.empty())
              complete_clear();
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
              dle_write(fmt::format("\x1b[{}E", dle_context.completion[0].size()));
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
              complete_clear();
            move_to_end();
            break;
          case SpecialKey::CTRL_F:
            edit_right();
            break;
          case SpecialKey::CTRL_K:
            if(!dle_context.completion.empty())
              complete_clear();
            dle_context.line.erase(dle_context.pos);
            break;
          case SpecialKey::CTRL_L:
            if(!dle_context.completion.empty())
              complete_clear();
            clear_screen();
            break;
          case SpecialKey::LINE_FEED:
          case SpecialKey::ENTER:
            if (dle_context.line.empty())
              dle_context.history.pop_back();
            else
            {
              dle_context.history.back().cmd = dle_context.line;
              dle_context.history.back().timestamp = utils::get_timestamp();
            }
            if(!dle_context.completion.empty())
            {
              complete_clear();
            }
            edit_refresh_line(false);
            return 0;
            break;
          case SpecialKey::CTRL_N:
            edit_down();
            break;
          case SpecialKey::CTRL_P:
            edit_up();
            break;
          case SpecialKey::CTRL_T:
            if(!dle_context.completion.empty())
              complete_clear();
            if (dle_context.pos != 0)
            {
              std::swap(dle_context.line[dle_context.pos],
                        dle_context.line[dle_context.pos - 1]);
            }
            break;
          case SpecialKey::CTRL_U:
            if(!dle_context.completion.empty())
              complete_clear();
            dle_context.line.clear();
            dle_context.pos = 0;
            break;
          case SpecialKey::CTRL_W:
          {
            if(!dle_context.completion.empty())
              complete_clear();
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
        edit_refresh_line();
      }
    }
    return -1;
  }

  std::string read_line(const std::string &prompt)
  {
    dle_context.prompt = prompt;
    fmt::print(dle_context.prompt);
    edit_line();
    fmt::print("\n");
    return dle_context.line;
  }
}