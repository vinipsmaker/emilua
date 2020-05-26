-- This test ensures that fibers whose joinable fiber handles got collected
-- will have their stacktrace printed to stderr (if err'ed)

local println = require('println')

collectgarbage("stop")

spawn(function()
    println('secondary fiber starts')
    error('tag', 0)
end)

println('secondary fiber spawned')
this_fiber.yield()
println('about to gc')
collectgarbage("collect")
println('end of main fiber')
