-- Multiple waiters

local sleep = require('time').sleep
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
    print('201')
    m:lock()
    print('202')
    assert_eq(state, 0)
    c:wait(m)
    assert_eq(state, 1)
    print('203')
    state = 2
    m:unlock()
end)

f = spawn(function()
    print('301')
    m:lock()
    print('302')
    assert_eq(state, 0)
    c:wait(m)
    assert_eq(state, 2)
    print('303')
    m:unlock()
end)

this_fiber.yield()
print('101')
m:lock()
print('102')
state = 1
m:unlock()
print('103')
c:notify_one()
print('104')
sleep(0.05)
f:interrupt()
