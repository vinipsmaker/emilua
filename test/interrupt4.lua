-- interrupt() while join()'ing

local sleep_for = require('sleep_for')

f = spawn(function()
    sleep_for(0.01)
    print('foo')
end)

spawn(function() f:interrupt() end):detach()
f:join()
print(f.interruption_caught)
