local println = require('println')

scope(function()
    scope_cleanup_push(function() println('foo') end)
    error('bar', 0)
end)
println('baz')
