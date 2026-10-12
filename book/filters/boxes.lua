--[[
boxes.lua: labelled boxes for the recurring parts of a chapter.

  ::: {.box title="Break it"}
  Text...
  :::

LaTeX gets a tcolorbox with the title; HTML gets <div class="box"> with a
heading line. Other formats get the content with a bold title line.
]]

function Div(el)
  if not el.classes:includes("box") then return nil end
  local title = el.attributes["title"] or ""
  if FORMAT:match("latex") then
    local open = pandoc.RawBlock("latex",
      "\\begin{tcolorbox}[enhanced,breakable,colback=boxbg,colframe=boxframe," ..
      "boxrule=0.4pt,arc=2pt,left=6pt,right=6pt,top=4pt,bottom=4pt," ..
      "fonttitle=\\sffamily\\bfseries\\small,coltitle=boxtitle,colbacktitle=boxbg," ..
      "title={" .. title .. "},attach title to upper={\\par\\smallskip}]")
    local close = pandoc.RawBlock("latex", "\\end{tcolorbox}")
    local out = {open}
    for _, b in ipairs(el.content) do out[#out + 1] = b end
    out[#out + 1] = close
    return out
  elseif FORMAT:match("html") then
    local head = pandoc.RawBlock("html", '<div class="box"><div class="boxtitle">' .. title .. '</div>')
    local out = {head}
    for _, b in ipairs(el.content) do out[#out + 1] = b end
    out[#out + 1] = pandoc.RawBlock("html", "</div>")
    return out
  else
    local out = {pandoc.Para({pandoc.Strong({pandoc.Str(title)})})}
    for _, b in ipairs(el.content) do out[#out + 1] = b end
    return out
  end
end

-- "# Part I: Foundations {.part}" becomes \part in LaTeX; other formats keep
-- it as a level-1 heading.
function Header(el)
  if el.level == 1 and el.classes:includes("part") and FORMAT:match("latex") then
    return pandoc.RawBlock("latex", "\\part{" .. pandoc.utils.stringify(el.content) .. "}")
  end
  return nil
end
