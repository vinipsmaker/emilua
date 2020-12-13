print('Fiber ' .. this_fiber.id)
spawn(function() print('Fiber ' .. this_fiber.id) end):join()
