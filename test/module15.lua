local println = require('println')

coroutine.wrap(function()
    println('a')
    require('./module15_foo')
    println('c')
end)()
