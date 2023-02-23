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
    m:lock()
    print('202')
    assert_eq(state, 0)
    c:wait(m)
    assert_eq(state, 1)
    print('203')
    m:unlock()
end)

print('101')
this_fiber.yield()
this_fiber.yield()
this_fiber.yield()
print('102')
m:lock()
print('103')
state = 1
m:unlock()
this_fiber.yield()
this_fiber.yield()
this_fiber.yield()
print('104')
c:notify_one()
print('105')
