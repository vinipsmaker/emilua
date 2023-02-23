-- If the waiter has no suspension points between the point where it access the
-- condition and the call to wait(), the notifier doesn't need to mutate the
-- condition through a mutex.

local mutex = require('mutex')
local cond = require('condition_variable')

local m = mutex.new()
local c = cond.new()

local state = 0

function assert_eq(x, y)
    if x ~= y then
        error('critical check assert_eq() has failed [' .. tostring(x) ..
              ' != ' .. tostring(y) .. ']', 0)
    end
end

spawn(function()
    print('201')
    m:lock();this_fiber.forbid_suspend()
    print('202')
    assert_eq(state, 0)
    this_fiber.allow_suspend();c:wait(m)
    assert_eq(state, 1)
    print('203')
    m:unlock()
end)

print('101')
this_fiber.yield()
print('102')
state = 1
c:notify_one()
print('103')
