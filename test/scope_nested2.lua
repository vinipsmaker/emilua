scope(function()
    scope_cleanup_push(function() print('foo1a') end)
    scope_cleanup_push(function() print('foo1b') end)

    scope(function()
        print('bar2')
    end)

    print('bar1')
end)
print('baz1')
