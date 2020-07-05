local println = require('println')

scope(function()
    scope_cleanup_push(function() println('foo') end)
    println('bar')
end)
println('baz')
