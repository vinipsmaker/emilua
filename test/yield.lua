spawn(function()
    print('second')
    this_fiber.yield()
    print('fourth')
end):detach()

print('first')
this_fiber.yield()
print('third')
