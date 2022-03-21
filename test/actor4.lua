if _CONTEXT == 'main' then
    local ch = spawn_vm('.')

    -- The number of yield()s required is an implementation detail and may
    -- change among versions.
    --
    -- The point is: The worker must finish before the main actor calls
    -- `ch:send()`. That's what we're testing: can the sender detect the
    -- receiver is already gone?
    this_fiber.yield()

    print('A')
    ch:send('foobar')
else
    assert(_CONTEXT == 'worker')
    print('B')
end
