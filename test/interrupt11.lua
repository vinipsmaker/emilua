-- Read invalid interruption_caught on detached
-- not-yet-had-a-chance-to-run-fiber
local println = require('println')

fib = spawn(function() end)
fib:detach()
println(tostring(fib.interruption_caught))
