scope(function()
    scope_cleanup_push(function() error('foo') end)
    scope_cleanup_pop()
    print('bar')
end)
print('baz')
