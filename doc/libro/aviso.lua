-- aviso.lua: los bloques ::: {.aviso} se dibujan como un recuadro (entorno `aviso` de header.tex)
function Div(el)
  if el.classes:includes("aviso") then
    return { pandoc.RawBlock("latex", "\\begin{aviso}"), el, pandoc.RawBlock("latex", "\\end{aviso}") }
  end
end
