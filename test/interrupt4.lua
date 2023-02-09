-- interrupt() while join()'ing

local sleep = require('sleep')

f = spawn(function()
    sleep(0.01)
    print('foo')
end)

spawn(function() f:interrupt() end):detach()
f:join()
print(f.interruption_caught)
