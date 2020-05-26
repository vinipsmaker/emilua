{
    gsub(/Fiber 0x[[:xdigit:]]+/, "Fiber 0x0")
    while (i = index($0, TEST)) {
        $0 = substr($0, 1, i - 1) "input" substr($0, i + length(TEST))
    }
    getline expected < (TEST ".out")
    if ($0 != expected) {
        print "Expected:\n\t" expected "\nGot:\n\t" $0 > "/dev/stderr"
        err = 1
    }
}
END { exit err }
