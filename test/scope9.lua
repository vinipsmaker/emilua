-- Coroutine scopes are fiber independent

co = coroutine.create(function()
    scope_cleanup_push(function() print('c1') end)
    scope_cleanup_push(function() print('c2') end)
    scope_cleanup_push(function() print('c3') end)
    coroutine.yield()
    scope_cleanup_pop()
    coroutine.yield()
    scope_cleanup_pop()
    coroutine.yield()
    scope_cleanup_pop()
end)

spawn(function()
    scope_cleanup_push(function() print('p1') end)
    coroutine.resume(co)
end):detach()

spawn(function()
    scope_cleanup_push(function() print('p2') end)
    coroutine.resume(co)
end):detach()

spawn(function()
    scope_cleanup_push(function() print('p3') end)
    coroutine.resume(co)
end):detach()

coroutine.resume(co)
