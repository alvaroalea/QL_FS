#!/bin/sh
# lang_check.sh <idioma>: cada <x>_es.h debe tener su <x>_<idioma>.h con los mismos nombres y la misma "forma"
# (numero de %, \n, | y ^ en cada texto). Solo sh y awk.
lang=${1:-en}
rc=0
for es in $(find src -name '*_es.h' | sort); do
    f=${es%_es.h}_$lang.h
    if [ ! -f "$f" ]; then echo "FALTA el fichero $f"; rc=1; continue; fi
    awk -v F="$f" '
    function shape(v,   n, s, c) { s = ""; c = v; n = gsub(/%[-+ #0-9.*hlL]*[a-zA-Z%]/, "", c); s = n; n = gsub(/\\n/, "", c); s = s "/" n
        n = gsub(/\|/, "", c); s = s "/" n; n = gsub(/\^/, "", c); s = s "/" n; return s }
    /^#define S_/ { name = $2; v = $0; sub(/^#define [A-Za-z0-9_]+ +/, "", v)
        if (FILENAME != F) { es[name] = shape(v); order[++n] = name }
        else { seen[name] = 1; if (!(name in es)) { print F ": sobra " name; bad = 1 }
               else if (es[name] != shape(v)) { print F ": " name " cambia la forma (%/\\n/|/^): " es[name] " -> " shape(v); bad = 1 } } }
    END { for (i = 1; i <= n; i++) if (!(order[i] in seen)) { print F ": falta " order[i]; bad = 1 } exit bad }' "$es" "$f" || rc=1
done
[ $rc = 0 ] && echo "idioma $lang: todos los textos coinciden"
exit $rc
