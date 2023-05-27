dish.alias = {
    ls = "ls --color=tty",
    grep = "grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"
}

function prompt()
    io.write(string.format("\n[1;34m#[0m [1;36m%s[0m in [1;33m%s[0m\n",
            dish.environment.USERNAME, dish.environment.PWD))
    if(dish.environment.UID == "0") then
        io.write("[1;31m# [0m")
    else
        io.write("[1;31m$ [0m")
    end
end

dish.prompt = prompt;