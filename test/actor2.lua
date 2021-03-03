local println = require('println')
local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send(inbox)
    println('A')
    local m = inbox:recv()
else
    assert(_CONTEXT == 'worker')
    -- We use global scope to avoid garbage collection until the very end.
    ch = inbox:recv()

    -- The number of yield()s required is an implementation detail and may
    -- change among versions.
    --
    -- The point is: The worker must finish only after the main actor already
    -- called `inbox:recv()`. That's what we're testing: will the last sender
    -- awake the the fiber stalled in the `recv()` operation?
    this_fiber.yield()
    this_fiber.yield()
    println('B')
end
