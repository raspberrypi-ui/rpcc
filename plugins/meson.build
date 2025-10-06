sources = files (
	'hello_world.c',
)

gtk = dependency('gtk+-3.0')
deps = [ gtk ]

shared_module('hello_world', sources, dependencies: deps, install: false)
