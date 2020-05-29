println = require('println')

coroutine.wrap(function()
    println(type(coroutine.running()))
end)()
println(type(coroutine.running()))
