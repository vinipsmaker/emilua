local println = require('println')

scope(function()
    scope_cleanup_push(function() error('foo') end)
    scope_cleanup_pop()
    println('bar')
end)
println('baz')
