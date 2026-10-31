#!/bin/bash
# check_glibc.sh file...: prints the newest GLIBC and GLIBCXX symbol versions each file needs,
# and fails if any needs a glibc newer than MAX_GLIBC (2.28, manylinux_2_28) or a libstdc++
# newer than MAX_GLIBCXX (3.4.25, the gcc 8 libstdc++ of RHEL 8, also in Ubuntu 18.04), or
# links libgomp dynamically. A mex file (a shared object with no program interpreter) may use
# libgomp.so: the static libgomp.a is not position-independent, and MATLAB and Octave load a
# libgomp of their own (as in mcx). Adapted from mcx; files that do not exist are skipped.

MAX_GLIBC=${MAX_GLIBC:-2.28}
MAX_GLIBCXX=${MAX_GLIBCXX:-3.4.25}
fail=0

newer() {  # newer A B: true if version A > version B
    [ -n "$1" ] && [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -1)" != "$2" ]
}

for f in "$@"; do
    [ -f "$f" ] || continue
    syms=$(objdump -T "$f" 2>/dev/null)
    glibc=$(echo "$syms" | grep -oE 'GLIBC_[0-9.]+' | sed 's/GLIBC_//' | sort -uV | tail -1)
    glibcxx=$(echo "$syms" | grep -oE 'GLIBCXX_[0-9.]+' | sed 's/GLIBCXX_//' | sort -uV | tail -1)
    needed=$(objdump -p "$f" 2>/dev/null | awk '/NEEDED/{print $2}' | tr '\n' ' ')
    status=ok

    if newer "$glibc" "$MAX_GLIBC"; then
        status="FAIL: needs GLIBC_$glibc"
        fail=1
    fi

    if newer "$glibcxx" "$MAX_GLIBCXX"; then
        status="$status; FAIL: needs GLIBCXX_$glibcxx"
        fail=1
    fi

    if echo "$needed" | grep -qE 'libgomp' && readelf -l "$f" 2>/dev/null | grep -q INTERP; then
        status="$status; FAIL: links libgomp dynamically"
        fail=1
    fi

    printf '%-40s GLIBC_%-6s GLIBCXX_%-8s %s\n    needs: %s\n' "$f" "${glibc:--}" "${glibcxx:--}" "$status" "$needed"
done

exit $fail
