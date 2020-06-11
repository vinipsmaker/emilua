-- Ensure interruptions won't swallow a condition signal that was already sent.
-- From IEEE Std 1003.1, 2013 Edition, Standard for Information Technology --
-- Portable Operating System Interface (POSIX), The Open Group Base
-- Specifications Issue 7, Copyright (C) 2013 by the Institute of Electrical and
-- Electronics Engineers, Inc and The Open Group:
--
-- > A thread that has been unblocked because it has been canceled while blocked
-- > in a call to pthread_cond_timedwait() or pthread_cond_wait() shall not
-- > consume any condition signal that may be directed concurrently at the
-- > condition variable if there are other threads blocked on the condition
-- > variable.
--
-- The property ensured by this test case also goes along the mindset described
-- in the section "IO objects" from the "Interruption API" manual.

local println = require('println')
local mutex = require('mutex')
local cond = require('cond')

local m = mutex.new()
local c = cond.new()

f = spawn(function()
    m:lock()
    println('foo')
    local ok, err = pcall(function() c:wait(m) end)
    println(tostring(ok))
    m:unlock()
    if not ok then
        error(err, 0)
    end
    println('baz')
end)

spawn(function()
    m:lock()
    println('foo2')
    local ok = pcall(function() c:wait(m) end)
    println(tostring(ok))
    m:unlock()
    println('baz2')
end)

this_fiber.yield()
this_fiber.yield()
println('bar')
f:interrupt()
c:notify_one()
