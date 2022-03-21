co = coroutine.create(function()
    return 1, 2, 3
end)

x = {coroutine.resume(co)}
print(#x)
print(x[2])
print(x[3])
print(x[4])

co = coroutine.create(function()
    error({'tag'}, 0)
end)

x = {coroutine.resume(co)}
print(#x)
print(x[1])
print(x[2][1])
