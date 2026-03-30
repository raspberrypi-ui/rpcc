project ('rpcc','c')

share_dir = join_paths(get_option('prefix'), 'share')
resource_dir = join_paths(share_dir, 'rpcc')
ui_dir = join_paths(resource_dir, 'ui')
desktop_dir = join_paths(share_dir, 'applications')

i18n = import('i18n')
gettext_package = meson.project_name()

add_global_arguments('-Wno-unused-result', language : 'c')

add_project_arguments('-DGETTEXT_PACKAGE="' + gettext_package + '"', language : [ 'c', 'cpp' ])
add_project_arguments('-DPACKAGE_LOCALE_DIR="' + share_dir + '/locale"', language : [ 'c', 'cpp' ])
add_project_arguments('-DPACKAGE_DATA_DIR="' + resource_dir + '"', language : [ 'c', 'cpp' ])

wayland_client = dependency('wayland-client')
wayland_protos = dependency('wayland-protocols')
subdir('proto')
deps = [ wf_protos ]

subdir('po')
subdir('src')
subdir('data')
