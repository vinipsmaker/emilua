scope(function()
    scope_cleanup_push(function() print('foo') end)
    error('bar', 0)
end)
print('baz')
