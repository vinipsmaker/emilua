function sanitize_record()
{
    $0 = gensub(/(Fiber|VM) 0x[[:xdigit:]]+\>/, "\\1 0x0", "g")
    while (i = index($0, printed_path)) {
        $0 = substr($0, 1, i - 1) "input" substr($0, i + length(printed_path))
    }
}

BEGIN {
    FS = "\n"
    printed_path = TEST
    if (length(printed_path) > 55) {
        printed_path = "..." substr(printed_path, length(printed_path) - 51)
    }
}
{
    sanitize_record()
    switch (getline expected <(TEST ".out")) {
    default:
        printf "Error while reading %s.out: %s\n", TEST, ERRNO >"/dev/stderr"
        err = 1
        exit
    case 0:
        printf "Expected EOF\nGot (LINE %i):\n\t%s\n", NR, $0 >"/dev/stderr"
        while (getline > 0) {
            sanitize_record()
            print "\t" $0 >"/dev/stderr"
        }
        err = 1
        break
    case 1:
        if ($0 != expected) {
            printf "Expected (LINE %i):\n\t%s\nGot:\n\t%s\n", NR, expected, $0 \
                >"/dev/stderr"
            err = 1
        }
    }
}
END {
    if (err) {
        exit err
    }

    switch (getline expected <(TEST ".out")) {
    default:
        printf "Error while reading %s.out: %s\n", TEST, ERRNO >"/dev/stderr"
        exit 1
    case 1:
        printf "Expected (LINE %i):\n", NR + 1 >"/dev/stderr"
        do {
            print "\t" expected >"/dev/stderr"
        } while (getline expected <(TEST ".out") > 0)
        print "Got EOF" >"/dev/stderr"
        exit 1
    case 0:
        exit
    }
}
