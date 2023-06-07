dish.alias = {
    ls ="ls --color=tty",
    grep ="grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"
}

-- bold = 1, faint, italic, underline, slow_blink, rapid_blink, color_reverse,
-- fg_black = 30, fg_red, fg_green, fg_yellow, fg_blue, fg_magenta, fg_cyan, fg_white,
-- bg_black = 40, bg_red, bg_green, bg_yellow, bg_blue, bg_magenta, bg_cyan, bg_white
dish.style = {
    cmd = dish.effects.fg_blue,
    arg = dish.effects.fg_cyan,
    string = dish.effects.fg_yellow,
    env = dish.effects.fg_green,
    error = dish.effects.fg_red,
    hint = dish.effects.faint,
    info = dish.effects.fg_magenta
}
function ys_prompt()
    p = string.format("\n[1;34m#[0m [36m%s[0m in [1;33m%s[0m [%s]\n",
            dish.environment.USERNAME, dish.environment.PWD, os.date("%H:%M:%S",os.time()))
    if(dish.environment.UID =="0") then
        return p .."[1;31m# [0m"
    else
        return p .."[1;31m$ [0m"
    end
end

function my_prompt()
    local p = string.format("\n[36m%s[0m in [1;32m%s[0m> ",
            dish.environment.USERNAME, dish_get_shrunk_path())
    return p
end


dish.prompt = my_prompt


git_table = { { "add", "Add file contents to the index" },
                    { "merge", "Join two or more development histories together" },
                    { "apply", "Apply a patch on a git index file and a working tree" },
                    { "mergetool", "Run merge conflict resolution tools to resolve merge conflicts" },
                    { "archive", "Create an archive of files from a named tree" },
                    { "merge-base", "Find as good common ancestors as possible for a merge" },
                    { "bisect", "Find the change that introduced a bug by binary search" },
                    { "mv", "Move or rename a file, a directory, or a symlink" },
                    { "blame", "Show what revision and author last modified each line of a file" },
                    { "prune", "Prune all unreachable objects from the object database" },
                    { "branch", "List, create, or delete branches" },
                    { "pull", "Fetch from and merge with another repository or a local branch" },
                    { "checkout", "Checkout and switch to a branch" },
                    { "push", "Update remote refs along with associated objects" },
                    { "cherry", "Find commits yet to be applied to upstream [upstream [head]]" },
                    { "range-diff", "Compare two commit ranges (e.g. two versions of a branch" },
                    { "cherry-pick", "Apply the change introduced by an existing commit" },
                    { "rebase", "Forward-port local commits to the updated upstream head" },
                    { "clean", "Remove untracked files from the working tree" },
                    { "reflog", "Manage reflog information" },
                    { "clone", "Clone a repository into a new directory" },
                    { "remote", "Manage set of tracked repositories" },
                    { "commit", "Record changes to the repository" },
                    { "reset", "Reset current HEAD to the specified state" },
                    { "config", "Set and read git configuration variables" },
                    { "restore", "Restore working tree files" },
                    { "count-objects  (Count unpacked number of objects and their disk consumption" },
                    { "revert", "Revert an existing commit" },
                    { "describe", "Give an object a human readable name based on an available ref" },
                    { "rev-parse", "Pick out and massage parameters" },
                    { "diff", "Show changes between commits, commit and working tree, etc" },
                    { "rm", "Remove files from the working tree and the index" },
                    { "difftool", "Open diffs in a visual tool" },
                    { "shortlog", "Show commit shortlog" },
                    { "fetch", "Download objects and refs from another repository" },
                    { "show", "Shows the last commit of a branch" },
                    { "filter-branch", "Rewrite branches" },
                    { "show-branch", "Shows the commits on branches" },
                    { "format-patch", "Generate patch series to send upstream" },
                    { "stash", "Stash away changes" },
                    { "gc", "Cleanup unnecessary files and optimize the local repository" },
                    { "status", "Show the working tree status" },
                    { "grep", "Print lines matching a pattern" },
                    { "submodule", "Initialize, update or inspect submodules" },
                    { "help", "Display help information about Git" },
                    { "switch", "Switch to a branch" },
                    { "init", "Create an empty git repository or reinitialize an existing one" },
                    { "tag", "Create, list, delete or verify a tag object signed with GPG" },
                    { "log", "Show commit logs" },
                    { "whatchanged", "Show logs with difference each commit introduces" },
                    { "ls-files", "Show information about files in the index and the working tree" },
                    { "worktree", "Manage multiple working trees" }
}

function match(src, pattern)
    local ret = {}
    for i, _ in ipairs(src) do
        beg, _ = string.find(src[i][1], pattern)
        if(beg == 1) then
            ret[#ret + 1] = src[i]
        end
    end
    return ret
end

function hint(before_pattern, pattern)
    if(before_pattern =="git") then
        local h = match(git_table, pattern)[1][1]
        if(h) then
            return string.sub(h, #pattern + 1)
        end
    end
end

dish.enable_hint = true
dish.hint = hint;

function complete(before_pattern, pattern)
    if(before_pattern =="git") then
        return match(git_table, pattern)
    end
end
dish.complete = complete;

function load_zsh_history(path)
    local ret = {}
    file = io.open(path,"r")
    assert(file,"Open file failed.")
    local line = file:read()
    while line do
        if(line ~="" and string.sub(line, 1, 1) ==":") then
            if(string.sub(line, -1, -1) ~="\\") then
                if(ignore) then ignore = False end
                cmd_pos, _ = string.find(line,";")
                assert(cmd_pos,"Unknown history format:\n" .. line)
                cmd = string.sub(line, cmd_pos + 1, -1)
                ts_pos, _ = string.find(line,":", 3)
                assert(ts_pos,"Unknown history format:\n" .. line)
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
