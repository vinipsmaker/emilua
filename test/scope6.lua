scope(function()
    scope_cleanup_push(function() print('foobar') end)
    scope_cleanup_push(function() print('foo') end)
    scope_cleanup_push(function() print('bar') end)
    scope_cleanup_push(function() print('baz') end)
    scope_cleanup_pop()
    scope_cleanup_pop(false)
    scope_cleanup_pop(true)
    print('qux')
end)
