co = coroutine.create(function()
    coroutine.yield(1, 2, 3)
end)

x = {coroutine.resume(co)}
print(#x)
print(type(x[2]))
print(x[2])
print(type(x[3]))
print(x[3])
print(type(x[4]))
print(x[4])
