local json = require('json')

local x = json.decode('{"hello": "world"}')
print(x.hello)

local heavily_nested_json = json.decode('{"a":{"b":{"c":{"d":{"e":{"f":{"g":{"h":{"i":{"j":{"k":{"l":{"m":{"n":{"o":{"p":{"q":{"r":{"s":{"t":{"u":{"v":{"w":{"x":{"y":{"z1":{},"z2":{}}}}}}}}}}}}}}}}}}}}}}}}}}}')
print(heavily_nested_json.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z1)
print(heavily_nested_json.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z2)
