println = require('println')

co = coroutine.create(function()
    return 1, 2, 3
end)

x = {coroutine.resume(co)}
println(tostring(#x))
println(tostring(x[2]))
println(tostring(x[3]))
println(tostring(x[4]))

co = coroutine.create(function()
    error({'tag'}, 0)
end)

x = {coroutine.resume(co)}
println(tostring(#x))
println(tostring(x[1]))
println(tostring(x[2][1]))
