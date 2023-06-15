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
    // DEBUG
    // std::cout.flush();
  }

  template<typename ...Args>
  void dle_write(const tiny_utf8::string& fmt, Args&& ...args)
  {
    dle_write(fmt::format(fmt.cpp_str(), std::forward<Args>(args)...));
  }

  size_t dle_pos_width()
  {
    return utils::display_width(dle_context.line.begin(),
                                dle_context.line.begin() + dle_context.pos);
  }

  void dle_init()
  {
    dle_context.history_pos = 0;
    dle_context.last_cols = 0;
    dle_context.completion_pos_line = 0;
    dle_context.completion_pos_column = 0;
    dle_context.completion_show_line_pos = 0;
    dle_context.completion_show_line_size = 0;

    // Put the terminal to raw mode for line editing.
    dish_context.tmodes.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    dish_context.tmodes.c_cflag |= (CS8);
    dish_context.tmodes.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    dish_context.tmodes.c_cc[VMIN] = 1;
    dish_context.tmodes.c_cc[VTIME] = 0;
    tcsetattr(dish_context.terminal, TCSAFLUSH, &dish_context.tmodes);

    dle_context.searching_completion = false;
  }

  int dle_screen_height()
  {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return (w.ws_row != 0 ? w.ws_row : 8);
  }

  int dle_screen_width()
  {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return (w.ws_col != 0 ? w.ws_col : 128);
  }

  tiny_utf8::string get_color(int c)
  {
    return "\033[" + std::to_string(c) + "m";
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

  std::tuple<tiny_utf8::string, tiny_utf8::string> split_path(const tiny_utf8::string& pattern)
  {
    if(pattern.empty()) return {"", ""};
    if(pattern == "/") return {"/", ""};
    tiny_utf8::string path, filename;
    auto i = pattern.rfind('/');
    if(i == tiny_utf8::string::npos)
      filename = pattern;
    else
    {
      path = pattern.substr(0, i + 1);
      filename = pattern.substr(i + 1);
    }
    return {path, filename};
  }

  std::tuple<tiny_utf8::string, tiny_utf8::string> get_pattern()
  {
    tiny_utf8::string before_pattern;
    tiny_utf8::string pattern;
    if (dle_context.line.back() != ' ')
    {
      auto i = dle_context.line.rfind(' ', dle_context.pos);
      if (i != tiny_utf8::string::npos && i + 1 < dle_context.line.length())
      {
        pattern = dle_context.line.substr(i + 1);
        before_pattern = dle_context.line.substr(0, i);
      }
      else
        pattern = dle_context.line;
    }
    else
    {
      before_pattern = dle_context.line;
      if(!before_pattern.empty()) before_pattern.pop_back();
    }
    return {before_pattern, pattern};
  }

  tiny_utf8::string get_hint()
  {
    auto [before_pattern, pattern] = get_pattern();
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
    // filesystem hint
    if (auto files = utils::match_files_and_dirs(pattern); !files.empty())
    {
      auto [path, filename] = split_path(pattern);
      tiny_utf8::string ret;
      if (!path.empty() && utils::begin_with(files[0], path))
        ret = files[0].substr(path.length());
      else
        ret = files[0];
      if (!filename.empty() && utils::begin_with(ret, filename))
        ret = ret.substr(filename.length());
      return ret;
    }
    return "";
  }
  void complete_refresh();
  void cmdline_refresh(bool with_hints)
  {
    // move to begin and clear the line
    if (dle_context.last_cols != 0)
      dle_write("\x1b[{}D\x1b[K", dle_context.last_cols);
    else
      dle_write("\x1b[K");
    // the current line
    dle_write(highlight_line());
    // hint
    dle_context.hint.clear();
    if (with_hints)
    {
      dle_context.hint = get_hint();
      dle_write(utils::effect(dle_context.hint,
                              static_cast<utils::Effect>(dish_context.lua_state["dish"]["style"]["hint"].get<int>())));
    }
    // move cursor back
    auto cursor_col = dle_pos_width();
    auto curr_col = utils::display_width(dle_context.hint, dle_context.line);
    if(curr_col != cursor_col)
      dle_write("\x1b[{}D", curr_col - cursor_col);
    // save pos for next refresh
    dle_context.last_cols = cursor_col;
  }
  void edit_refresh_line(bool with_hints)
  {
    if (dle_context.completion.empty())
      cmdline_refresh(with_hints);
    else
      complete_refresh();
  }

  std::vector<CompletionCandidate> get_argument_candidate()
  {
    std::vector<CompletionCandidate> ret;
    // lua complete
    if (sol::protected_function h = dish_context.lua_state["dish"]["complete"]; h.valid())
    {
      auto [before_pattern, pattern] = get_pattern();
      dle_context.complete_pattern = pattern;
      auto res = h(before_pattern.cpp_str(), dle_context.complete_pattern.cpp_str());
      if (res.valid() && res.get_type() == sol::type::table)
      {
        for (auto &r: res.get<sol::table>())
        {
          if (r.second.get_type() != sol::type::table)
            break;
          if (auto lhs = r.second.as<sol::table>()[1], rhs = r.second.as<sol::table>()[2];
              lhs.valid() && rhs.valid() && lhs.get_type() == sol::type::string && rhs.get_type() == sol::type::string)
          {
            tiny_utf8::string selection;
            if (auto raw_lua = r.second.as<sol::table>()[3];
                raw_lua.valid() && raw_lua.get_type() == sol::type::string)
              selection = raw_lua.get<std::string>();
            else
              selection = lhs.get<std::string>();
            ret.emplace_back(
                    CompletionCandidate{
                            .completion = lhs.get<std::string>(),
                            .info = rhs.get<std::string>(),
                            .selection = selection});
          }
        }
      }
    }
    if (ret.empty())
    {
      auto f = utils::match_files_and_dirs(dle_context.complete_pattern);
      for (auto &r: f)
      {
        auto [path, filename] = split_path(dle_context.complete_pattern);
        tiny_utf8::string raw;
        raw = path + r;
        ret.emplace_back(CompletionCandidate{.completion = raw,
                                             .info = "",
                                             .selection = r});
      }
      dle_context.complete_pattern = "";
    }
    return ret;
  }

  std::vector<CompletionCandidate> get_cmd_candidate()
  {
    std::vector<CompletionCandidate> ret;
    dle_context.complete_pattern = dle_context.line.substr(0, dle_context.pos);
    auto cmds = utils::match_command(dle_context.complete_pattern);
    for (auto &r: cmds)
    {
      tiny_utf8::string info;
      if (r.type == utils::CommandType::executable_file || r.type == utils::CommandType::executable_link)
        info = utils::to_string(r.type) + ", " + utils::get_human_readable_size(r.file_size);
      else
        info = utils::to_string(r.type);
      ret.emplace_back(CompletionCandidate{.completion = r.name,
                                           .info = info,
                                           .selection = r.name});
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
    size_t columns = dle_screen_width() / (max_size + 2);
    if(columns == 0) columns = 1;
    size_t lines = std::ceil(static_cast<double>(total) / static_cast<double>(columns));
    columns -= (lines * columns - total) / lines;

    dle_context.completion.insert(dle_context.completion.end(), columns, {});
    for (auto &r: dle_context.completion) r.insert(r.end(), lines, {"", ""});
    if(lines > dle_screen_height() - 3)
      dle_context.completion_show_line_size = dle_screen_height() - 3;
    else
      dle_context.completion_show_line_size = lines;
    return {columns, lines};
  }

  void complete_select();
  void complete_clear();

  int complete_line()
  {
    auto info_color = static_cast<utils::Effect>(dish_context.lua_state["dish"]["style"]["info"]);
    auto err_color = static_cast<utils::Effect>(dish_context.lua_state["dish"]["style"]["error"]);
    std::vector<CompletionCandidate> items;
    // command
    if (auto it = std::find(dle_context.line.begin(),
                            dle_context.line.begin() + dle_context.pos, ' ');
        it == dle_context.line.end())
    {
      items = get_cmd_candidate();
    }
    else
    {
      items = get_argument_candidate();
    }
    if (items.empty())
      return 0;
    else if(items.size() == 1)
    {
      dle_context.completion = {{CompletionDisplay{items[0].completion, ""}}};
      complete_select();
      return 0;
    }
    auto max_size_item = std::max_element(items.cbegin(), items.cend(),
                                          [](auto &&a, auto &&b) {
                                            return (utils::display_width(a.info, a.selection) <
                                                    utils::display_width(b.info, b.selection));
                                          });
    size_t max_width = utils::display_width(max_size_item->info, max_size_item->selection) + 2;
    max_width = (std::min)(max_width, static_cast<size_t>(dle_screen_width()));
    auto [columns, lines] = complete_init(items.size(), max_width);
    size_t line = 0;
    size_t column = 0;
    for (auto &r: items)
    {
      tiny_utf8::string selection = r.selection;
      if(utils::display_width(r.selection, r.info) + 2 <= max_width)
      {
        // if selection is shorter than max_width, then add info.
        if(!r.info.empty())
        {
          selection.insert(selection.end(),
                           tiny_utf8::string(max_width - utils::display_width(r.selection, r.info) - 2, ' '));
          selection += "(" + utils::effect(r.info, info_color) + ")";
        }
        else
        {
          selection.insert(selection.end(),
                           tiny_utf8::string(max_width - utils::display_width(r.selection), ' '));
        }
      }
      else
      {
        // otherwise we need to cut info or selection
        if(!r.info.empty() && utils::display_width(r.selection) + 3 <= max_width)
        {
          tiny_utf8::string short_info;
          short_info = r.info.substr(0, max_width - utils::display_width(r.selection) - 2);
          selection += "(" + utils::effect(short_info, info_color) + utils::effect("~", err_color);
        }
        else
        {
          if(auto w = utils::display_width(selection); w <= max_width)
            selection.insert(selection.end(), tiny_utf8::string(max_width - w, ' '));
          else
            selection = selection.substr(0, max_width - 1) + utils::effect("~", err_color);
        }
      }
      auto s = utils::display_width(selection);
      dle_context.completion[column][line] = {r.completion, selection};
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


  int get_prompt_columns()
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
      ret += utils::display_width(dle_context.prompt.substr(last_line + 1));
    }
    return ret;
  }

  int get_pos_columns()
  {
    return get_prompt_columns() + dle_context.last_cols;
  }

  void complete_apply()
  {
    const auto &completion = dle_context.completion[dle_context.completion_pos_column]
                                                               [dle_context.completion_pos_line].completion;
    if (dle_context.pos != 0 && dle_context.line[dle_context.pos - 1] != ' ')
    {
      int b = dle_context.pos;
      while (b > 0 && dle_context.line[b] != ' ')
        --b;
      if (dle_context.line[b] == ' ') ++b;
      dle_context.line.erase(b, dle_context.pos - b);
      dle_context.pos = b;
    }
    dle_context.line.insert(dle_context.pos, completion);
    dle_context.pos += completion.length();
    dle_context.last_cols = dle_pos_width();
  }

  void complete_refresh()
  {
    size_t completion_col = utils::display_width(dle_context.completion[0][0].selection);
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
        const auto &line = dle_context.completion[c][l].selection;
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
          dle_write("{}", out);
          dle_write("\x1b[{}D\x1b[1B", completion_col);
        }
        else // Padding
          dle_write("\x1b[1D\x1b[1B");
      }
      if (c < columns - 1)
        dle_write("\x1b[{}A\x1b[{}C", screen_lines, completion_col + 2);
    }

    if (dle_context.searching_completion)
      complete_apply();
    // move cursor back
    dle_write("\x1b[{}F", screen_lines + 1);
    if (auto i = get_pos_columns(); i != 0)
      dle_write("\x1b[{}C", i);
    cmdline_refresh(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void complete_clear()
  {
    // When there is only an item, we have noting to clear.
    if (!dle_context.completion.empty() && (dle_context.completion.size() != 1 || dle_context.completion[0].size() != 1))
    {
      size_t screen_lines = 1 + std::min(dle_context.completion_show_line_size, dle_context.completion[0].size());
      for (size_t i = 0; i < screen_lines; ++i)
        dle_write("\x1b[B\x1b[2K");
      dle_write("\x1b[{}F", screen_lines);
    }
    if (auto i = get_pos_columns(); i != 0)
    {
      dle_write("\x1b[{}D\x1b[{}C",
                get_prompt_columns() + utils::display_width(dle_context.line), i);
    }
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
    if (dle_context.line[dle_context.pos - 1] != '/')
    {
      dle_context.line.insert(dle_context.line.begin() + dle_context.pos, ' ');
      ++dle_context.pos;
    }
  }

  void move_to_beginning()
  {
    if (dle_context.pos == 0) return;
    dle_write("\x1b[{}D", dle_pos_width());
    dle_context.pos = 0;
    dle_context.last_cols = 0;
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
    auto origin_width = dle_pos_width();
    dle_context.pos = dle_context.line.length();
    dle_context.last_cols = dle_pos_width();
    dle_write("\x1b[{}C", dle_context.last_cols - origin_width);
    if (refresh) edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
  }

  void move_to_word_beginning()
  {
    auto origin = dle_pos_width();
    if (dle_context.line[dle_context.pos - 1] == ' ')
      --dle_context.pos;
    // curr is not space
    while (dle_context.pos > 0 && dle_context.line[dle_context.pos] == ' ')
      --dle_context.pos;
    // prev is space or begin
    while (dle_context.pos > 0 && dle_context.line[dle_context.pos - 1] != ' ')
      --dle_context.pos;
    dle_context.last_cols = dle_pos_width();
    dle_write("\x1b[{}D", origin - dle_context.last_cols);
  }

  void move_to_word_end()
  {
    auto origin = dle_pos_width();
    // curr is not space
    while (dle_context.pos < dle_context.line.length() && dle_context.line[dle_context.pos] == ' ')
      ++dle_context.pos;
    // next is space or end
    while (dle_context.pos < dle_context.line.length() && dle_context.line[dle_context.pos] != ' ')
      ++dle_context.pos;
    dle_context.last_cols = dle_pos_width();
    dle_write("\x1b[{}C", dle_context.last_cols - origin);
  }

  void move_left()
  {
    if (dle_context.pos > 0)
    {
      auto origin = dle_pos_width();
      --dle_context.pos;
      dle_context.last_cols = dle_pos_width();
      dle_write("\x1b[{}D", origin - dle_context.last_cols);
    }
  }

  void move_right()
  {
    if (dle_context.pos < dle_context.line.length())
    {
      auto origin = dle_pos_width();
      ++dle_context.pos;
      dle_context.last_cols = dle_pos_width();
      dle_write("\x1b[{}C", dle_context.last_cols - origin);
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

  int utf8_len(const std::string& s)
  {
    int j, len = 0;
    for (int i = 0; s[i] != 0; i++)
    {
      if (s[i] & 0x80)
      {
        j = 1;
        while ((s[++i] & 0x80) && !(s[i] & 0x40)) j++;
        len += 2;
        i--;
      }
      else
        len++;
    }
    return len;
  }


  // the core of Dish Line Editor
  int edit_line()
  {
    dle_context.line.clear();
    dle_context.last_cols = 0;
    dle_context.pos = 0;
    dle_context.history.emplace_back(History{"", ""});
    dle_context.history_pos = dle_context.history.size() - 1;
    std::string codepoint_buf;
    while (true)
    {
      char buf;
      std::cin.read(&buf, 1);
      if (is_special_key(static_cast<int>(buf)))
      {
        SpecialKey key = static_cast<SpecialKey>(buf);
        if (key == SpecialKey::TAB)
        {
          // 1
          if (dle_context.completion.empty())
          {
            complete_line();
          }
          else
          {
            // 2
            if (!dle_context.searching_completion)
              dle_context.searching_completion = true;
            // 3
            else
              complete_down();
          }
          edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
          continue;
        }
        switch (key)
        {
          case SpecialKey::CTRL_A:
            if (!dle_context.completion.empty())
              complete_select();
            move_to_beginning();
            break;
          case SpecialKey::CTRL_B:
            edit_left();
            break;
          case SpecialKey::CTRL_C:
            dle_context.line.clear();
            dle_context.history.pop_back();
            dish_context.lua_state["last_foreground_ret"] = 127;
            dle_write("\033[40;37m^C\033[0m");
            if (!dle_context.completion.empty())
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
            if (!dle_context.completion.empty())
              complete_select();
            move_to_end();
            break;
          case SpecialKey::CTRL_F:
            edit_right();
            break;
          case SpecialKey::CTRL_K:
            if (!dle_context.completion.empty())
              complete_select();
            edit_delete();
            break;
          case SpecialKey::CTRL_L:
            if (!dle_context.completion.empty())
              complete_select();
            clear_screen();
            break;
          case SpecialKey::LINE_FEED:
          case SpecialKey::ENTER:
            if (!dle_context.completion.empty())
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
            if (!dle_context.completion.empty())
              complete_select();
            if (dle_context.pos != 0)
            {
              auto tmp = dle_context.line[dle_context.pos];
              dle_context.line[dle_context.pos] = dle_context.line[dle_context.pos - 1];
              dle_context.line[dle_context.pos - 1] = tmp;
            }
            break;
          case SpecialKey::CTRL_U:
            if (!dle_context.completion.empty())
              complete_select();
            break;
          case SpecialKey::CTRL_W:
          {
            if (!dle_context.completion.empty())
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
            if (!dle_context.completion.empty())
              complete_clear();
            edit_backspace();
            break;
          default:
            fmt::println(stderr, "\nWARNING: Ignored unrecognized key '{}'.", buf);
            dle_context.history.pop_back();
            dish_context.lua_state["last_foreground_ret"] = -1;
            return -1;
            break;
        }
        edit_refresh_line(dish_context.lua_state["dish"]["enable_hint"]);
      }
      else
      {
        if(isprint(buf) || is_special_key(buf) || iscntrl(buf))
          dle_context.line.insert(dle_context.pos++, buf);
        else
        {
          codepoint_buf += buf;
          if (utf8_len(codepoint_buf) < codepoint_buf.size())
          {
            dle_context.line.insert(dle_context.pos++, codepoint_buf);
            codepoint_buf.clear();
          }
          else
            continue ;
        }
        // Input a character should close the history searching and competion.
        dle_context.searching_history_pattern.clear();
        if (!dle_context.completion.empty())
          complete_clear();
        dle_context.completion_pos_line = 0;
        dle_context.completion_pos_column = 0;
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