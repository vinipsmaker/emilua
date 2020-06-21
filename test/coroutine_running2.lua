println = require('println')

spawn(function()
    println(tostring(coroutine.running()))
end):join()
