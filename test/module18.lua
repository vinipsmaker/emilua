print('Fiber ' .. this_fiber.id)
require('module18_foo')
print('Fiber ' .. this_fiber.id)
spawn(function() print('Fiber ' .. this_fiber.id) end):join()
