local println = require('println')

pcall(function()
    scope_cleanup_push(function() println('foo') end)
    println('bar')
end)
println('baz')
