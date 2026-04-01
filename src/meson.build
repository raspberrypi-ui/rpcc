sources = files (
    'rpcc.c',
    'activate.c'
)

gtk = dependency ('gtk+-3.0')
deps = [ deps, gtk ]

proj_args = '-DPLUGIN_PATH="' + get_option ('prefix') + '/' + get_option('libdir') + '/rpcc/"'

executable ('rpcc', sources, dependencies: deps, c_args : proj_args, install: true, export_dynamic: true)
