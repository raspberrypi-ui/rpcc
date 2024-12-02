sources = files (
    'rpcc.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
deps = [ gtk ]

executable ('rpcc', sources, dependencies: deps, install: true)
