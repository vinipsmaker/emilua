local println = require('println')

xpcall(
    function()
        scope_cleanup_push(function() println('foo') end)
        println('bar')
    end,
    function(e) return e end
)
println('baz')
