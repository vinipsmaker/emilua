println = require('println')

co = coroutine.create(function()
    coroutine.yield(1, 2, 3)
end)

x = {coroutine.resume(co)}
println(tostring(#x))
println(type(x[2]))
println(tostring(x[2]))
println(type(x[3]))
println(tostring(x[3]))
println(type(x[4]))
println(tostring(x[4]))
