print(this_fiber.is_main)
spawn(function() print(this_fiber.is_main) end):join()
