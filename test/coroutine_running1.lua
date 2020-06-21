println = require('println')

coroutine.wrap(function()
    println(tostring(coroutine.running()))
end)()
println(tostring(coroutine.running()))
