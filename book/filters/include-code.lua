--[[
include-code.lua: fill code blocks from the chapter directories, so the book
always shows the code that is built and tested.

  ```{.cpp include="ch05-listview/src/session_item.h"}
  ```                                   -- the whole file

  ```{.cpp include="ch05-listview/src/app_window.cpp" fn="void AppWindow::on_factory_bind("}
  ```                                   -- one function, with the // comment block above it

  ```{.cpp include="ch07-actions/src/app_window.cpp" from="GMenu *top" to="g_object_unref(top);"}
  ```                                   -- an excerpt, both lines included

  Add nocomment="1" to fn= to leave out the comment block above the function.
  Paths are relative to the repository root (the parent of book/).
  A missing file or anchor is a build error, never a silent empty block.
]]

local root = os.getenv("BOOK_REPO_ROOT") or ".."

local function read_lines(path)
  local f = io.open(root .. "/" .. path, "r")
  if not f then
    error("include-code: cannot open " .. path)
  end
  local lines = {}
  for line in f:lines() do
    lines[#lines + 1] = line
  end
  f:close()
  return lines
end

local function starts_with(s, prefix)
  return s:sub(1, #prefix) == prefix
end

-- Anchors are matched against lines with leading whitespace removed, because
-- pandoc attribute values cannot start with spaces.
local function anchored(line, prefix)
  return starts_with((line:gsub("^%s+", "")), prefix)
end

local function find_line(lines, prefix, from_index, path)
  for i = from_index or 1, #lines do
    if anchored(lines[i], prefix) then
      return i
    end
  end
  error("include-code: '" .. prefix .. "' not found in " .. path)
end

local function count(s, ch)
  local _, n = s:gsub(ch, "")
  return n
end

-- From the line starting with `prefix` to the brace that closes it.
local function extract_fn(lines, prefix, path, with_comment)
  local i = find_line(lines, prefix, 1, path)
  local first = i
  if with_comment then
    while first > 1 and anchored(lines[first - 1], "//") do
      first = first - 1
    end
  end
  local depth, started = 0, false
  local k = i
  while k <= #lines do
    local l = lines[k]
    depth = depth + count(l, "{") - count(l, "}")
    if l:find("{", 1, true) then started = true end
    if started and depth <= 0 then break end
    if not started and l:match(";%s*$") then break end
    k = k + 1
  end
  local out = {}
  for j = first, k do out[#out + 1] = lines[j] end
  return out
end

function CodeBlock(el)
  local path = el.attributes["include"]
  if not path then return nil end
  local lines = read_lines(path)
  local out
  if el.attributes["fn"] then
    out = extract_fn(lines, el.attributes["fn"], path, el.attributes["nocomment"] == nil)
  elseif el.attributes["from"] then
    local a = find_line(lines, el.attributes["from"], 1, path)
    local b = el.attributes["to"] and find_line(lines, el.attributes["to"], a, path) or #lines
    out = {}
    for j = a, b do out[#out + 1] = lines[j] end
  else
    out = lines
  end
  local had_part = el.attributes["fn"] ~= nil or el.attributes["from"] ~= nil
  -- Drop leading and trailing blank lines.
  while #out > 0 and out[1]:match("^%s*$") do table.remove(out, 1) end
  while #out > 0 and out[#out]:match("^%s*$") do table.remove(out) end
  -- Excerpts from inside a function: remove the common indentation.
  local indent = nil
  for _, l in ipairs(out) do
    if not l:match("^%s*$") then
      local lead = #l:match("^( *)")
      if indent == nil or lead < indent then indent = lead end
    end
  end
  if indent and indent > 0 then
    for i, l in ipairs(out) do out[i] = l:sub(indent + 1) end
  end
  el.text = table.concat(out, "\n")
  -- Keep the source path as a caption-like attribute for the HTML edition.
  el.attributes["include"] = nil
  el.attributes["fn"] = nil
  el.attributes["from"] = nil
  el.attributes["to"] = nil
  el.attributes["nocomment"] = nil
  -- Label the block with where it came from, so readers can open the file.
  local tag
  if FORMAT:match("latex") then
    tag = pandoc.RawBlock("latex", "\\codesource{" .. path:gsub("_", "\\_") .. "}")
  elseif FORMAT:match("html") then
    tag = pandoc.RawBlock("html", '<div class="codesource">' .. path .. '</div>')
  else
    tag = pandoc.Para({pandoc.Emph({pandoc.Str(path)})})
  end
  return {tag, el}
end
