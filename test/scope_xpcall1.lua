xpcall(
    function()
        scope_cleanup_push(function() print('foo') end)
        print('bar')
    end,
    function(e) return e end
)
print('baz')
