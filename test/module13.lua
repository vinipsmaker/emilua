-- That's the most important/basic module system test. It checks whether modules
-- are isolated from each other.

local println = require('println')

foo = 'foo'
require('module13_foo')
println(foo)
