sleep = require('sleep')

coroutine.resume(coroutine.create(function()
    sleep(0.01)
    print('foo')
end))

print('bar')
