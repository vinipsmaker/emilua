-- Coroutine scopes are fiber independent

local println = require('println')

co = coroutine.create(function()
    scope_cleanup_push(function() println('c1') end)
    scope_cleanup_push(function() println('c2') end)
    scope_cleanup_push(function() println('c3') end)
    coroutine.yield()
    scope_cleanup_pop()
    coroutine.yield()
    scope_cleanup_pop()
    coroutine.yield()
    scope_cleanup_pop()
end)

spawn(function()
    scope_cleanup_push(function() println('p1') end)
    coroutine.resume(co)
end):detach()

spawn(function()
    scope_cleanup_push(function() println('p2') end)
    coroutine.resume(co)
end):detach()

spawn(function()
    scope_cleanup_push(function() println('p3') end)
    coroutine.resume(co)
end):detach()

coroutine.resume(co)
