println = require('println')

spawn(function()
    println(type(coroutine.running()))
end):join()
