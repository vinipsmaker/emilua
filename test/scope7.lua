spawn(function()
    scope_cleanup_push(function() print('foo1') end)
    scope_cleanup_push(function() print('foo2') end)
end)

coroutine.wrap(function()
    scope_cleanup_push(function() print('bar1') end)
    scope_cleanup_push(function() print('bar2') end)
end)()

scope_cleanup_push(function() print('baz1') end)
scope_cleanup_push(function() print('baz2') end)
