local println = require('println')

scope(function()
    scope_cleanup_push(function() println('foo1a') end)
    scope_cleanup_push(function() println('foo1b') end)

    scope(function()
        println('bar2')
    end)

    println('bar1')
end)
println('baz1')
