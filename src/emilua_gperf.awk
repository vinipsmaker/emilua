#!/usr/bin/gawk --file
#
# emilua_gperf.awk
#
# Written in 2023 by Vinícius dos Santos Oliveira <vini.ipsmaker@gmail.com>
#
# To the extent possible under law, the author(s) have dedicated all copyright
# and related and neighboring rights to this software to the public domain
# worldwide. This software is distributed without any warranty.
#
# You should have received a copy of the CC0 Public Domain Dedication along with
# this software. If not, see
# <http://creativecommons.org/publicdomain/zero/1.0/>.

function gperf(context_index    , i, proc, input, out) {
    proc = GPERF_BIN " --language=C++ --enum --readonly-tables --struct-type " \
        "--class-name=" symbol_prefix context_index
    input = sprintf( \
        "struct word_type_%s%i { const char* name; %s; };\n%%%%\n",
        symbol_prefix, context_index,
        context[context_index, "param"])

    for (i in context[context_index, "pairs"]) {
        input = input i ", EMILUA_GPERF_DETAIL_VALUE" \
            context[context_index, "pairs"][i] "_\n"
    }

    input = input "%%\n"

    printf "%s", input |& proc
    close(proc, "to")
    proc |& getline out
    if (close(proc) != 0) {
        print "gperf exited with failuren" >"/dev/stderr"
        exit 1
    }
    if (out !~ /duplicates = 0/) {
        print "WARNING: Duplicates found" >"/dev/stderr"
    }

    for (i in context[context_index, "pairs"]) {
        sub("EMILUA_GPERF_DETAIL_VALUE" context[context_index, "pairs"][i] "_",
            context["pairs"][context[context_index, "pairs"][i]], out)
    }

    output_header = output_header \
        "namespace emilua::gperf::detail {\n" out \
        "} // namespace emilua::gperf::detail\n"
    if (length(context[context_index, "default_value"]) > 0) {
        return sprintf( \
            " ::emilua::gperf::detail::value_or(::emilua::gperf::detail::" \
            "%1$s%2$i::in_word_set((%3$s).data(), (%3$s).size()), %4$s)",
            symbol_prefix, context_index,
            context[context_index, "symbol"],
            context[context_index, "default_value"])
    } else {
        return sprintf( \
            " ::emilua::gperf::detail::make_optional(::emilua::gperf::" \
            "detail::%1$s%2$i::in_word_set((%3$s).data(), (%3$s).size()))",
            symbol_prefix, context_index,
            context[context_index, "symbol"])
    }
}

function process_arguments(    i, levels, ret) {
    if (substr($0, 1, 1) != "(") {
        printf "Open parens expected. Got: `%s`\n", substr($0, 1, 1) \
            >"/dev/stderr"
        exit 1
    }

    levels = 1
    for (i = 2 ; levels != 0 ; ++i) {
        if (substr($0, i, 1) == "(") {
            ++levels
        } else if (substr($0, i, 1) == ")") {
            --levels
        }
    }
    ret = substr($0, 2, i - 3)
    $0 = substr($0, i)
    return ret
}

function process_param(context_index) {
    context[context_index, "param"] = process_arguments()
}

function process_default_value(context_index    , aux, value) {
    value = process_arguments()
    aux = $0
    $0 = value
    value = ""
    while (match($0, /EMILUA_GPERF_BEGIN/)) {
        value = value substr($0, 1, RSTART - 1)
        $0 = substr($0, RSTART + RLENGTH)
        value = value process_gperf_block()
    }
    value = value $0
    $0 = aux
    context[context_index, "default_value"] = value
}

function process_pair(context_index    , saved_input, value, matches) {
    value = process_arguments()
    match(value, /"([^"]*)"/, matches)
    value = substr(value, index(value, ",") + 1)

    if (matches[1] in context[context_index, "pairs"]) {
        printf("ERROR: Duplicate value found: `%s`\n", matches[1]) \
            >"/dev/stderr"
        exit 1
    }

    saved_input = $0
    $0 = value
    value = ""
    while (match($0, /EMILUA_GPERF_BEGIN/)) {
        value = value substr($0, 1, RSTART - 1)
        $0 = substr($0, RSTART + RLENGTH)
        value = value process_gperf_block()
    }
    value = value $0
    $0 = saved_input

    gsub(/&/, "\\\\&", value)
    context[context_index, "pairs"][matches[1]] = context["next_value"]
    context["pairs"][context["next_value"]] = value
    ++context["next_value"]
}

function process_decls_block(    symbol, idx, saved_input, value, matches) {
    symbol = "EMILUA_GPERF_DECLS_END(" process_arguments() ")"
    idx = index($0, symbol)
    value = substr($0, 1, idx - 1)
    $0 = substr($0, idx + length(symbol))

    saved_input = $0
    $0 = value
    value = ""
    while (match($0, /EMILUA_GPERF_BEGIN/)) {
        value = value substr($0, 1, RSTART - 1)
        $0 = substr($0, RSTART + RLENGTH)
        value = value process_gperf_block()
    }
    value = value $0
    $0 = saved_input

    match(value, /EMILUA_GPERF_NAMESPACE\(([[:alpha:]_][[:alnum:]_]*)?\)/,
          matches)
    gsub(/EMILUA_GPERF_NAMESPACE\(([[:alpha:]_][[:alnum:]_]*)?\)/, "", value)
    if (matches[1, "length"] > 0) {
        output_header = sprintf( \
            "%1$snamespace %2$s {\n%3$s\n} // namespace %2$s\n",
            output_header, matches[1], value)
    } else {
        output_header = output_header value
    }
}

function process_gperf_block(    context_index, symbol, matches) {
    context_index = context["next"]++

    # initialize array
    context[context_index, "pairs"][0] = 0
    delete context[context_index, "pairs"][0]

    symbol = process_arguments()
    if (symbol !~ /^([[:alpha:]_][[:alnum:]_]*)?$/) {
        printf "Bad name for block: `%s`\n", symbol >"/dev/stderr"
        exit 1
    }
    context[context_index, "symbol"] = symbol

    while (match( \
        $0, /EMILUA_GPERF_(BEGIN|END|PARAM|DEFAULT_VALUE|PAIR)/, matches \
    )) {
        $0 = substr($0, RSTART + RLENGTH)
        if (matches[1] == "BEGIN") {
            print "GPERF block cannot appear here" >"/dev/stderr"
            exit 1
        } else if (matches[1] == "END") {
            if (process_arguments() != symbol) {
                printf "Mismatched blocks. Expected `%s`\n", symbol \
                    >"/dev/stderr"
                exit 1
            }
            return gperf(context_index)
        } else if (matches[1] == "PARAM") {
            process_param(context_index)
        } else if (matches[1] == "DEFAULT_VALUE") {
            process_default_value(context_index)
        } else if (matches[1] == "PAIR") {
            process_pair(context_index)
        }
    }
}

BEGIN {
    RS = "^$"
    FS = "\0"
    getline
    output_header = "#include <cstring>\n\
\n\
namespace emilua::gperf::detail { using std::size_t; using std::strcmp; }\n\
\n"
    output_body = ""
    symbol_prefix = FILENAME
    gsub(/[^[:alpha:]_]+/, "_", symbol_prefix)
    symbol_prefix = "gperf_" symbol_prefix "_"
    context["next"] = 1
    context["next_value"] = 1
    while (match($0, /EMILUA_GPERF_(BEGIN|DECLS_BEGIN)/)) {
        output_body = output_body substr($0, 1, RSTART - 1)
        if (substr($0, RSTART, RLENGTH) == "EMILUA_GPERF_DECLS_BEGIN") {
            $0 = substr($0, RSTART + RLENGTH)
            process_decls_block()
        } else {
            $0 = substr($0, RSTART + RLENGTH)
            output_body = output_body process_gperf_block()
        }
    }
    output_body = output_body $0
    $0 = ""
    printf("%s%s", output_header, output_body) >ARGV[ARGC - 1]
}
