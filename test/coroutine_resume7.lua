println = require('println')
sleep_for = require('sleep_for')

coroutine.resume(coroutine.create(function()
    sleep_for(10)
    println('foo')
end))

println('bar')
