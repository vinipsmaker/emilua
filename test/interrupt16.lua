-- Interrupt on join

local println = require('println')

fib = spawn(function()
    local f2 = spawn(function()
        fib:interrupt()
    end)
    println('foo')
    local ok = pcall(function()
        f2:join()
        println('bar')
    end)
    println(tostring(ok))
    local ok = pcall(function() f2:join() end)
    println(tostring(ok))
    this_fiber.disable_interruption()
    f2:join()
    println('baz')
    f2:join() --< now invalid
    println('qux')
end)