dish.alias = {
    ls = "ls --color=tty",
    grep = "grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"
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