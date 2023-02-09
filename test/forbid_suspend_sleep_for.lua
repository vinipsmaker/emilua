local sleep = require('time').sleep

this_fiber.forbid_suspend()
sleep(0.001)
