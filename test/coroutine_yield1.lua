co = coroutine.create(function()
    coroutine.yield('foo')
    return 'bar'
end)

function helper(ret, val)
    print(ret)
    print(val)
    return coroutine.status(co) == 'suspended'
end

while helper(coroutine.resume(co)) do end

co = coroutine.create(function()
    error('tag', 0)
end)

while helper(coroutine.resume(co)) do end
