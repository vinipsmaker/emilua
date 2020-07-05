local println = require('println')

spawn(function()
    scope_cleanup_push(function() println('foo1') end)
    scope_cleanup_push(function() println('foo2') end)
end)

coroutine.wrap(function()
    scope_cleanup_push(function() println('bar1') end)
    scope_cleanup_push(function() println('bar2') end)
end)()

scope_cleanup_push(function() println('baz1') end)
scope_cleanup_push(function() println('baz2') end)
