name = 'testpl'

sources = files(
  'test.c'
)

gtkmm = dependency('gtkmm-3.0', version: '>=3.24')
deps = [ gtkmm ]

shared_module(name, sources,
        dependencies: deps,
        install: true,
        install_dir: get_option('libdir') / 'rpcc',
        c_args : '-DPLUGIN_NAME="' + name + '"'
)
