local mutex = require('mutex')
local sleep = require('sleep')

m = mutex.new()

spawn(function()
    m:lock()
    print('101')

    sleep(0.03)
    m:unlock()
    print('102')
end)

spawn(function()
    sleep(0.01)
    m:lock()
    print('201')
    m:unlock()
    print('202')
end)

spawn(function()
    sleep(0.02)
    m:lock()
    print('301')
    m:unlock()
    print('302')

    sleep(0.01)
    m:lock()
    print('311')
    m:unlock()
end)
