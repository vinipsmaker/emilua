-- If the waiter has no suspension points between the point where it access the
-- condition and the call to wait(), the notifier doesn't need to mutate the
-- condition through a mutex.

local println = require('println')
local mutex = require('mutex')
local cond = require('cond')

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
    println('201')
    m:lock();this_fiber.forbid_suspend()
    println('202')
    assert_eq(state, 0)
    this_fiber.allow_suspend();c:wait(m)
    assert_eq(state, 1)
    println('203')
    m:unlock()
end)

println('101')
this_fiber.yield()
println('102')
state = 1
c:notify_one()
println('103')
