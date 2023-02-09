sleep_for = require('sleep_for')

coroutine.resume(coroutine.create(function()
    sleep_for(0.01)
    print('foo')
end))

print('bar')
