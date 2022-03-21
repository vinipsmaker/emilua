print(this_fiber.local_.test)
this_fiber.local_.test = 'foo'
print(this_fiber.local_.test)

coroutine.wrap(function()
    print(this_fiber.local_.test)
end)()

spawn(function()
    print(this_fiber.local_.test)
    this_fiber.local_.test = 'bar'
    print(this_fiber.local_.test)
    this_fiber.yield()
    print(this_fiber.local_.test)
    print('end of secondary fiber')
end)
this_fiber.yield()

print(this_fiber.local_.test)
print('end of main fiber')
