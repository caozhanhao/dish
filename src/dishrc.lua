dish.alias = {
    ls = "ls --color=tty",
    grep = "grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"
}

-- clear = 0, bold, faint, italic, underline, slow_blink, rapid_blink, color_reverse,
-- fg_black = 30, fg_red, fg_green, fg_yellow, fg_blue, fg_magenta, fg_cyan, fg_white,
-- bg_black = 40, bg_red, bg_green, bg_yellow, bg_blue, bg_magenta, bg_cyan, bg_white
dish.style = {
    cmd = dish.effects.fg_blue,
    arg = dish.effects.fg_cyan,
    string = dish.effects.fg_yellow,
    env = dish.effects.fg_green,
    error = dish.effects.fg_red,
    hint = dish.effects.faint
}
function prompt()
    p = string.format("\n[1;34m#[0m [36m%s[0m in [1;33m%s[0m [%s]\n",
            dish.environment.USERNAME, dish.environment.PWD, os.date("%H:%M:%S",os.time()))
    if(dish.environment.UID == "0") then
        return p .. "[1;31m# [0m"
    else
        return p .. "[1;31m$ [0m"
    end
end
dish.prompt = prompt

function hint(pattern)
    if(pattern == "type ") then
        return "<command>"
    end
end
dish.hint = hint;

function complete(pattern)
    if(pattern == "git ") then
        return {
            "add", "checkout", "commit", "fetch", "init", "log", "ls-files",
            "merge", "mv", "pull", "push", "remote", "rm", "status"
        }
    end
end
dish.complete = complete;

function load_zsh_history(path)
    local ret = {}
    file = io.open(path, "r")
    assert(file, "Open file failed.")
    local line = file:read()
    while line do
        if(line ~= "" and string.sub(line, 1, 1) == ":") then
            if(string.sub(line, -1, -1) ~= "\\") then
                if(ignore) then ignore = False end
                cmd_pos, _ = string.find(line, ";")
                assert(cmd_pos, "Unknown history format:\n" .. line)
                cmd = string.sub(line, cmd_pos + 1, -1)
                ts_pos, _ = string.find(line, ":", 3)
                assert(ts_pos, "Unknown history format:\n" .. line)
                timestamp = string.sub(line, 3, ts_pos - 1)
                dish_add_history(timestamp, cmd)
            else
                print("Warning: Ignored multiline.(TODO: fix):\n", line);
            end
        end
        line = file:read()
    end
end
dish.func["load_zsh_history"] = load_zsh_history;
