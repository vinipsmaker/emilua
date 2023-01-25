BEGIN {
    FS = "\n"

    printf "%s", TEST |& ENVIRON["NORMALIZE_PATH_BIN"]
    close(ENVIRON["NORMALIZE_PATH_BIN"], "to")
    RS = "\0"
    ENVIRON["NORMALIZE_PATH_BIN"] |& getline printed_path[1]
    ENVIRON["NORMALIZE_PATH_BIN"] |& getline printed_path[2]
    RS = "\n"

    if (printed_path[1] == "") {
        printf "`normalize_path` not working: %s\n", ERRNO >"/dev/stderr"
        err = 1
        exit
    }

    if (PROCINFO["platform"] == "posix") {
        "uname -s" | getline uname_output
    }
}
!got_seed && NR == 1 && /^SEED=[0-9]+$/ {
    got_seed = 1
    NR = 0
    print
    next
}
{
    sanitize_record()
    switch (getline expected <(TEST ".out")) {
    default:
        # This error message might be printed twice, but that's okay and I don't
        # want to complicate the script's logic thanks to this alone. Better
        # print ERRNO twice than to lose an error event even once.
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
        exit
    case 1:
        if ($0 != expected) {
            printf "Expected (LINE %i):\n\t%s\nGot:\n\t%s\n", NR, expected, $0 \
                >"/dev/stderr"
            err = 1
        }
    }
}
END {
    switch (getline expected <(TEST ".out")) {
    default:
        # According to gawk, "[...] subsequent attempts to read from that file
        # result in an end-of-file indication". So, in theory, the same error
        # message shouldn't be printed twice, but, in practice, it does.
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
        break
    }

    exit err
}

function sanitize_record(    i, pattern, captures, input, output)
{
    # Normalize errno strings
    if (PROCINFO["platform"] == "mingw") {
        sub(/invalid argument/, "Invalid argument")
        sub(/operation not permitted/, "Operation not permitted")
        sub(/not supported/, "Operation not supported")
        sub(/device or resource busy/, "Device or resource busy")
        sub(/resource deadlock would occur/, "Resource deadlock avoided")
        sub(/'result out of range/, "'Numerical result out of range")
    } else if (uname_output == "Darwin") {
        sub(/Resource busy/, "Device or resource busy")
        sub(/Result too large/, "Numerical result out of range")
    }

    # Normalize runtime paths
    for (j in printed_path) {
        while (i = index($0, printed_path[j])) {
            $0 = substr($0, 1, i - 1) "input" \
                substr($0, i + length(printed_path[j]))
        }
    }

    # Normalize runtime addresses
    pattern = @/(Fiber|VM|mutex|thread:|coroutine|table:) 0x([[:xdigit:]]+\>)/
    for (input = $0 ; match(input, pattern, captures) ;
         input = substr(input, RSTART + RLENGTH)) {
        if (!(captures[2] in sanitize_record_addrs)) {
            sanitize_record_addrs[captures[2]] = \
                sprintf("0x%X", ++sanitize_record_addr_idx)
        }
        output = sprintf("%s%s%s %s",
                         output,                       #< append op
                         substr(input, 1, RSTART - 1), #< slice before match
                         # actual match work range
                         captures[1],
                         sanitize_record_addrs[captures[2]])
    }
    $0 = output input
}
