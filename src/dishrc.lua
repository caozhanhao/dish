dish.alias = {
    ls = "ls --color=tty",
    grep = "grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"
}


-- RED: 31, GREEN: 32, YELLOW: 33, BLUE: 34, PURPLE: 35, LIGHT_BLUE: 36, WHITE:37
dish.color = {
cmd = 34,
arg = 36,
string = 33,
env = 32,
error = 31
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
                cmd = string.sub(line, cmd_pos + 1)
                ts_pos, _ = string.find(line, ":")
                assert(ts_pos, "Unknown history format:\n" .. line)
                timestamp = string.sub(line, 3, ts_pos - 3)
                dish_add_history(timestamp, cmd)
            else
                print("Warning: Ignored multiline.(TODO: fix):\n", line);
            end
        end
        line = file:read()
    end
end

dish.prompt = prompt
dish.func["load_zsh_history"] = load_zsh_history;

