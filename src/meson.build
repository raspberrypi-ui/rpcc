sources = files (
    'rpcc.c',
    'activate.c'
)

gtk = dependency ('gtk+-3.0')
deps = [ deps, gtk ]

proj_args = [ '-DPLUGIN_PATH="' + get_option ('prefix') + '/' + get_option('libdir') + '/rpcc/"', '-DGETTEXT_PACKAGE="' + gettext_package + '"' ]

executable ('rpcc', sources, dependencies: deps, c_args : proj_args, install: true, export_dynamic: true)

plugin_name = 'rpcc_about'
udev=dependency('libudev')

shared_module (plugin_name, 'about.c', dependencies: [ deps, udev ], install: true,
  install_dir: get_option('libdir') / 'rpcc',
  c_args : [ '-DPACKAGE_DATA_DIR="' + resource_dir + '"', '-DGETTEXT_PACKAGE="' + plugin_name + '"', '-DPLUGIN_NAME="' + plugin_name + '"' ]
)
