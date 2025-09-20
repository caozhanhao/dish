dish.alias = {
	ls = "ls --color=tty",
	grep = "grep --color=auto --exclude-dir={.bzr,CVS,.git,.hg,.svn,.idea,.tox}"
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

function fast_dish_prompt()
	local p = string.format("\n[36m%s[0m in [1;32m%s[0m> ",
		dish.environment.USERNAME, dish_get_shrunk_path())
	return p
end

local last_pwd, last_git_info = nil, nil
local function shell_escape(s)
	if not s then return "''" end
	return "'" .. tostring(s):gsub("'", "'\\''") .. "'"
end

local function popen_trim(cmd)
	local f = io.popen(cmd)
	if not f then return nil end
	local out = f:read("*a")
	f:close()
	if not out then return nil end
	out = out:gsub("%s+$", "")
	if out == "" then return nil end
	return out
end

local function get_git_info_for_dir(dir)
	if dir == last_pwd and last_git_info then
		return last_git_info
	end
	last_pwd = dir

	local git = {}
	local dir_esc = shell_escape(dir)

	local inside = popen_trim("git -C " .. dir_esc .. " rev-parse --is-inside-work-tree 2>/dev/null")
	if not inside or inside ~= "true" then
		last_git_info = nil
		return nil
	end

	local branch = popen_trim("git -C " .. dir_esc .. " rev-parse --abbrev-ref HEAD 2>/dev/null")
	if not branch or branch == "HEAD" then
		branch = popen_trim("git -C " .. dir_esc .. " rev-parse --short HEAD 2>/dev/null") or "detached"
	end
	git.branch = branch

	last_git_info = git
	return git
end

function dish_prompt()
	local user = dish.environment.USERNAME
	local path = dish_get_shrunk_path()
	local cwd = dish.environment.PWD

	local git = get_git_info_for_dir(cwd)

	local git_segment = ""
	if git then
		git_segment = " (" .. git.branch .. ")"
	end

	local p = string.format("\n\27[36m%s\27[0m in \27[1;32m%s\27[0m%s> ", user, path, git_segment)
	return p
end

dish.prompt = dish_prompt

git_table = { { "add", "Add file contents to the index" , "add"},
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
				if(ignore) then
					ignore = False
				end
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

local function codepoint_to_utf8(n)
	if n <= 0x7f then
		return string.char(n)
	elseif n <= 0x7ff then
		local b1 = 0xc0 + math.floor(n / 0x40)
		local b2 = 0x80 + (n % 0x40)
		return string.char(b1, b2)
	elseif n <= 0xffff then
		local b1 = 0xe0 + math.floor(n / 0x1000)
		local b2 = 0x80 + (math.floor(n / 0x40) % 0x40)
		local b3 = 0x80 + (n % 0x40)
		return string.char(b1, b2, b3)
	elseif n <= 0x10ffff then
		local b1 = 0xf0 + math.floor(n / 0x40000)
		local b2 = 0x80 + (math.floor(n / 0x1000) % 0x40)
		local b3 = 0x80 + (math.floor(n / 0x40) % 0x40)
		local b4 = 0x80 + (n % 0x40)
		return string.char(b1, b2, b3, b4)
	else
		return "\239\191\189"
	end
end

local function unescape(s)
	if s == nil then return nil end

	if #s >= 2 then
		local f = string.sub(s, 1, 1)
		local l = string.sub(s, -1, -1)
		if (f == '"' or f == "'") and f == l then
			s = string.sub(s, 2, -2)
		end
	end

	s = s:gsub("\\u%{(%x+)%}", function(h)
		local cp = tonumber(h, 16)
		if not cp then return "" end
		return codepoint_to_utf8(cp)
	end)

	s = s:gsub("\\u(%x%x%x%x)", function(h)
		local cp = tonumber(h, 16)
		if not cp then return "" end
		return codepoint_to_utf8(cp)
	end)

	s = s:gsub("\\x(%x%x)", function(h)
		return string.char(tonumber(h, 16))
	end)

	s = s:gsub("\\([0-7][0-7]?[0-7]?)", function(o)
		return string.char(tonumber(o, 8))
	end)

	local simple = {
		["\\n"] = "\n",
		["\\r"] = "\r",
		["\\t"] = "\t",
		["\\b"] = "\b",
		["\\f"] = "\f",
		["\\v"] = "\11",
		["\\a"] = "\7",
		['\\"'] = '"',
		["\\'"] = "'",
		["\\\\"] = "\\",
	}
	s = s:gsub("\\[nrtbfvau\"'\\]", function(seq)
		return simple[seq] or seq
	end)

	return s
end
function load_fish_history(path)
	local file = io.open(path, "r")
	assert(file, "Open file failed: " .. tostring(path))
	local entry = nil
	local in_paths = false
	local function flush_entry()
		if not entry then
			return
		end
		if entry.cmd and entry.when then
			dish_add_history(entry.when, entry.cmd)
		else
			print("Warning: ignored incomplete entry (missing cmd or when):", entry.cmd, entry.when)
		end
		entry = nil
		in_paths = false
	end

	for rawline in file:lines() do
		local line = rawline
		if string.sub(line, -1) == "\r" then
			line = string.sub(line, 1, -2)
		end

		-- "- cmd: <something>"
		local cmd_val = string.match(line, "^%s*%-[%s]*cmd:%s*(.*)$")
		if cmd_val then
		-- Flush the previous entry
			if entry then
				flush_entry()
			end
			entry = { cmd = unescape(cmd_val), when = nil, paths = nil }
			in_paths = false
		else
			if entry and in_paths then
				local path_item = string.match(line, "^%s*%-[%s]*(.+)$")
				if path_item then
					entry.paths = entry.paths or {}
					table.insert(entry.paths, unescape(path_item))
				else
					in_paths = false
				end
			end

			if entry then
				local when_val = string.match(line, "^%s*when:%s*(%d+)%s*$")
				if when_val then
					entry.when = when_val
				end

				local paths_marker = string.match(line, "^%s*paths:%s*$")
				if paths_marker then
					in_paths = true
					entry.paths = entry.paths or {}
				end
			end
		end
	end

	if entry then
		flush_entry()
	end

	file:close()
end
dish.func["load_fish_history"] = load_fish_history;
