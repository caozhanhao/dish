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

#include "dish/line_editor.hpp"
#include "dish/utils.hpp"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>

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

    BACKSPACE =  127
  };

  bool is_special_key(int c)
  {
    return (c >= 0 && c <= 6) || (c >= 8 && c <= 14) || c == 16 || c == 20 || c == 21 || c == 23 || c == 27 || c == 127;
  }


  void dle_init()
  {
    dle_context.line = " ";
    dle_context.pos = 1;
    dle_context.history_pos = 0;
    dle_context.last_refresh_pos = 0;
  }

  int save_history(const std::string & path)
  {
    std::ofstream fs(path);
    if(!fs.good())
    {
      fmt::println(stderr, "Failed to save history. (path: '{}').", path);
      return -1;
    }
    for(auto& r : dle_context.history)
      fs << r.timestamp << ";" << r.cmd << "\n";
    fs.close();
    return 0;
  }

  int load_history(const std::string & path)
  {
    std::ifstream fs(path);
    if(!fs.good())
    {
      fmt::println(stderr, "Failed to load history. (path: '{}').", path);
      return -1;
    }
    std::string tmp;
    while(std::getline(fs, tmp))
    {
      if(tmp.find(';') == std::string::npos)
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

  int complete_line()
  {
    return 0;
  }

  void move_to_beginning()
  {
    if(dle_context.pos == 0) return;
    auto origin = dle_context.pos;
    dle_context.pos = 0;
    auto esc = fmt::format("\x1b[{}D", origin);
    dle_context.last_refresh_pos = dle_context.pos;
    std::cout.write(esc.c_str(),esc.size());
  }

  void move_to_end()
  {
    if(dle_context.pos == dle_context.line.size()) return;
    auto origin = dle_context.pos;
    dle_context.pos = dle_context.line.size();
    auto esc = fmt::format("\x1b[{}C", dle_context.pos - origin);
    dle_context.last_refresh_pos = dle_context.pos;
    std::cout.write(esc.c_str(),esc.size());
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
    std::cout.write(esc.c_str(),esc.size());
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
    std::cout.write(esc.c_str(),esc.size());
  }

  void move_left()
  {
    if (dle_context.pos > 0)
    {
      --dle_context.pos;
      dle_context.last_refresh_pos = dle_context.pos;
      std::cout.write("\x1b[D", 3);
    }
  }

  void move_right()
  {
    if(dle_context.pos < dle_context.line.size())
    {
      ++dle_context.pos;
      dle_context.last_refresh_pos = dle_context.pos;
      std::cout.write("\x1b[C", 3);
    }
  }

  void refresh_line()
  {
    std::string esc;
    // move to begin and clear the line
    if(dle_context.last_refresh_pos != 0)
      esc += fmt::format("\x1b[{}D\x1b[K", dle_context.last_refresh_pos);
    else
      esc += "\x1b[K";
    // the current line
    esc += dle_context.line;
    // move cursor back
    if(dle_context.pos < dle_context.line.size())
    {
      esc += fmt::format("\x1b[{}D", dle_context.line.size() - dle_context.pos);
    }
    // save pos for next refresh
    dle_context.last_refresh_pos = dle_context.pos;
    std::cout.write(esc.c_str(), esc.size());
  }

  void edit_delete()
  {
    if(dle_context.pos >= dle_context.line.size()) return;
    dle_context.line.erase(dle_context.pos, 1);
    refresh_line();
  }

  void edit_backspace()
  {
    if(dle_context.pos == 0) return;
    dle_context.line.erase(dle_context.pos - 1, 1);
    --dle_context.pos;
    refresh_line();
  }

  void edit_delete_next_word()
  {
    if(dle_context.pos == dle_context.line.size() - 1) return;
    auto i = dle_context.pos;
    // skip space
    while (i < dle_context.line.size() && dle_context.line[i] == ' ')
      ++i;
    // find end
    while (i < dle_context.line.size() && dle_context.line[i] != ' ')
      ++i;
    dle_context.line.erase(dle_context.pos + 1, i - dle_context.pos);
    refresh_line();
  }

  void edit_prev_history()
  {
    dle_context.history.back().cmd = dle_context.line;
    if(dle_context.history_pos != 0) dle_context.history_pos--;
    dle_context.line = dle_context.history[dle_context.history_pos].cmd;
    refresh_line();
  }

  void edit_next_history()
  {
    dle_context.history.back().cmd = dle_context.line;
    if(dle_context.history_pos != 0) dle_context.history_pos++;
    dle_context.line = dle_context.history[dle_context.history_pos].cmd;
    refresh_line();
  }

  void clear_screen()
  {
    std::cout.write("\x1b[H\x1b[2J",7);
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
          complete_line();
        }

        switch (key)
        {
          case SpecialKey::CTRL_A:
            move_to_beginning();
            break;
          case SpecialKey::CTRL_B:
            move_left();
            break;
          case SpecialKey::CTRL_C:
            dle_context.history.pop_back();
            dle_context.line.clear();
            dle_context.pos = 0;
            dish_context.lua_state["last_foreground_ret"] = 127;
            std::cout.write("\033[40;37m^C\033[0m", 14);
            return 0;
            break;
          case SpecialKey::CTRL_D:
            edit_delete();
            break;
          case SpecialKey::CTRL_E:
            move_to_end();
            break;
          case SpecialKey::CTRL_F:
            move_right();
            break;
          case SpecialKey::CTRL_K:
            dle_context.line.erase(dle_context.pos);
            break;
          case SpecialKey::CTRL_L:
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
            return 0;
            break;
          case SpecialKey::CTRL_N:
            edit_next_history();
            break;
          case SpecialKey::CTRL_P:
            edit_prev_history();
            break;
          case SpecialKey::CTRL_T:
            if(dle_context.pos != 0)
            {
              std::swap(dle_context.line[dle_context.pos],
                        dle_context.line[dle_context.pos - 1]);
            }
            break;
          case SpecialKey::CTRL_U:
            dle_context.line.clear();
            dle_context.pos = 0;
            break;
          case SpecialKey::CTRL_W:
          {
            if(dle_context.pos ==0) break;
            auto origin_pos = dle_context.pos;
            while (dle_context.pos > 0 && dle_context.line[dle_context.pos - 1] == ' ')
              dle_context.pos--;
            while (dle_context.pos > 0 && dle_context.line[dle_context.pos - 1] != ' ')
              dle_context.pos--;
            dle_context.line.erase(dle_context.pos, origin_pos - dle_context.pos);
            break;
          }
          case SpecialKey::ESC: // Escape Sequence
            char seq[3];
            std::cin.read(seq,1);
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
              std::cin.read(seq + 1,1);
              // esc [
              if (seq[0] == '[')
              {
                if (seq[1] >= '0' && seq[1] <= '9')
                {
                 std::cin.read(seq+2,1);
                  if (seq[2] == '~' && seq[1] == '3')
                  {
                    edit_delete();
                  }
                  else if(seq[2] == ';')
                  {
                    std::cin.read(seq,2);
                    if (seq[0] == '5' && seq[1] == 'C')
                      move_to_word_end();
                    if (seq[0] == '5' && seq[1] == 'D')
                      move_to_word_beginning();
                  }
                }
                else
                {
                  switch(seq[1])
                  {
                    case 'A':
                      edit_prev_history();
                      break;
                    case 'B':
                      edit_next_history();
                      break;
                    case 'C':
                      move_right();
                      break;
                    case 'D':
                      move_left();
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
                switch(seq[1])
                {
                  case 'A':
                    edit_prev_history();
                    break;
                  case 'B':
                    edit_next_history();
                    break;
                  case 'C':
                    move_right();
                    break;
                  case 'D':
                    move_left();
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
            edit_backspace();
            break;
          default:
            fmt::println(stderr, "\nWARNING: Ignored unrecognized key '{}'.", buf);
            return -1;
            break;
        }
      }
      else
      {
        dle_context.line.insert(dle_context.line.begin() + dle_context.pos, buf[0]);
        ++dle_context.pos;
        refresh_line();
      }
    }
    return -1;
  }

  std::string read_line(const std::string &prompt)
  {
    fmt::print(prompt);
    edit_line();
    fmt::print("\n");
    return dle_context.line;
  }
}
