-- Read invalid interruption_caught on not-yet-had-a-chance-to-run-fiber
local println = require('println')

fib = spawn(function() end)
println(tostring(fib.interruption_caught))
