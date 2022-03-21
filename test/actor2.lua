local inbox = require('inbox')

if _CONTEXT == 'main' then
    local ch = spawn_vm('.')
    ch:send(inbox)
    print('A')
    local m = inbox:receive()
else
    assert(_CONTEXT == 'worker')
    -- We use global scope to avoid garbage collection until the very end.
    ch = inbox:receive()

    -- The number of yield()s required is an implementation detail and may
    -- change among versions.
    --
    -- The point is: The worker must finish only after the main actor already
    -- called `inbox:receive()`. That's what we're testing: will the last sender
    -- awake the the fiber stalled in the `receive()` operation?
    this_fiber.yield()
    this_fiber.yield()
    print('B')
end
