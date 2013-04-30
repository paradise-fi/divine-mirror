#!/bin/sh

set -e

flatten() {
    sed -e "s|\\.|_|g" -e "s|/|_|g" -e "s|-|_|g" -e "s|+|_|g"
}

if test "$1" = "-l"; then
    var="$2_list"
    out="$var.cpp"
    rm -f $out
    shift 2
    echo "namespace divine {" >> $out

    for n in "$@"; do
        echo "extern const char *$(echo $n | flatten)_str;" >> $out
    done

    echo "struct stringtable { const char *n, *c; };" >> $out
    echo "stringtable ${var}[] = { " >> $out
    for n in "$@"; do
        var=$(echo "$n" | flatten)
        echo "{ \"$n\", ${var}_str }," >> $out
    done
    echo "{ nullptr, nullptr }" >> $out
    echo "};" >> $out
    echo "}" >> $out
else
    flat=$(echo $2 | flatten)
    sed -e '1i \
namespace divine { const char *'"${flat}"'_str = "\\' \
    -e 's,\\,\\\\,g' \
    -e 's,$,\\n\\,' \
    -e 's,",\\",g' \
    -e 's,wibble/test.h,cassert,' \
    -e '$a \
\"\; }' \
    < "$1/$2" > "${flat}_str.cpp"
fi
