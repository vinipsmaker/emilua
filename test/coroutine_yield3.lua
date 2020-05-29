println = require('println')

co = coroutine.create(function()
    coroutine.yield(1, 2, 3)
end)

x = {coroutine.resume(co)}
println(tostring(#x))
println(tostring(x[2] == 1))
println(tostring(x[3] == 2))
println(tostring(x[4] == 3))
