BEGIN {
    printed_path = TEST
    if (length(printed_path) > 55) {
        printed_path = "..." substr(printed_path, length(printed_path) - 51)
    }
}
{
    $0 = gensub(/(Fiber|VM) 0x[[:xdigit:]]+/, "\\1 0x0", "g")
    while (i = index($0, printed_path)) {
        $0 = substr($0, 1, i - 1) "input" substr($0, i + length(printed_path))
    }
    i = getline expected < (TEST ".out")
    if (i != 1) {
        if (i == 0) {
            print "Expected EOF\nGot:\n\t" $0 > "/dev/stderr"
        } else {
            print "Error while reading " TEST ".out" > "/dev/stderr"
        }
        exit 1
    }
    if ($0 != expected) {
        print "Expected (LINE " NR "):\n\t" expected "\nGot:\n\t" $0 > \
            "/dev/stderr"
        err = 1
    }
}
END {
    i = getline expected < (TEST ".out")
    if (i != 0) {
        if (i == 1) {
            print "Expected:\n\t" expected "\nGot EOF" > "/dev/stderr"
        } else {
            print "Error while reading " TEST ".out" > "/dev/stderr"
        }
        exit 1
    }
    exit err
}
