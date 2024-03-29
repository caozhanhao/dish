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
#ifndef DISH_JOB_HPP
#define DISH_JOB_HPP
#pragma once

#include "builtin.hpp"
#include "dish.hpp"
#include "utils.hpp"

#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include <string>
#include <list>
#include <vector>
#include <variant>
#include <memory>

namespace dish::job
{
  enum class RedirectType
  {
    overwrite, append, input, fd
  };
  
  class Redirect
  {
  private:
    RedirectType type;
    std::variant<int, tiny_utf8::string> redirect;
  public:
    Redirect() = default;
    template<typename T, typename = std::enable_if_t<!std::is_base_of_v<Redirect, std::decay_t<T>>>>
    Redirect(RedirectType type_, T &&redirect_) : type(type_), redirect(redirect_) {}
    
    bool is_description() const;
    
    tiny_utf8::string get_filename() const;
    
    int get_description() const;
    
    int get() const;
  };
  
  class Job;
  enum class ProcessType
  {
    unknown, builtin, lua_func,
    executable
  };

  class Process
  {
    friend class Job;
  private:
    Job *job_context;
    std::vector<tiny_utf8::string> args;
    tiny_utf8::string cmd_path;
  public:
    ProcessType type;
    int pid;
    int status;
    int exit_status;
    bool completed;
    bool stopped;
  public:
    Process()
        : pid(-1), status(-1), completed(false),
          stopped(false), exit_status(-1), type(ProcessType::unknown), job_context(nullptr){}

    void launch();
    
    void insert(tiny_utf8::string str);
    
    void clear();
    
    bool empty() const;
    
    void set_job_context(Job *job_);

    int find_cmd();

    std::vector<char *> get_args() const;
  };
  
  class Job
  {
    friend class Process;
  private:
    tiny_utf8::string command_str; //for message
    Redirect out;
    Redirect in;
    Redirect err;
    struct termios job_tmodes;
    pid_t cmd_pgid;
    bool background;
  public:
    std::vector<Process> processes;
    bool notified;
  public:
    Job() = default;
    Job(tiny_utf8::string cmd);
    
    int launch();
    
    void insert(const Process &scmd);
    
    void set_in(Redirect redirect);
    
    void set_out(Redirect redirect);
    
    void set_err(Redirect redirect);
    
    void set_background();

    void set_foreground();

    void put_in_foreground(int cont);
    
    void put_in_background(int cont);
    
    bool is_stopped();

    bool is_background();

    bool is_completed();

    bool is_builtin_or_lua();

    void wait();


    tiny_utf8::string format_job_info(const tiny_utf8::string &status);

    void continue_job();
    void update_status();
  private:
    int mark_status(int, int);
  };
}
#endif