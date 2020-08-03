project('patchmatrix', 'c', default_options : [
	'buildtype=release',
	'warning_level=1',
	'werror=false',
	'b_lto=false',
	'c_std=gnu11'])

nk_pugl = subproject('nk_pugl')

nk_pugl_dep = nk_pugl.get_variable('nk_pugl_gl')
cousine_regular_ttf = nk_pugl.get_variable('cousine_regular_ttf')

version = run_command('cat', 'VERSION').stdout().strip()

prefix = get_option('prefix')
datadir = get_option('datadir')
bindir = get_option('bindir')

appdir = join_paths(prefix, datadir, 'applications')
pdatadir = join_paths(prefix, datadir, 'patchmatrix', '')

add_project_arguments('-DPATCHMATRIX_VERSION="'+version+'"', language : 'c')
add_project_arguments('-DPATCHMATRIX_DATA_DIR="'+pdatadir+'"', language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')
add_project_arguments('-DPUGL_HAVE_GL', language : 'c')

conf_data = configuration_data()
conf_data.set('prefix', prefix)
conf_data.set('bindir', bindir)
conf_data.set('datadir', datadir)

cc = meson.get_compiler('c')

if cc.has_header('jack/metadata.h')
	add_project_arguments('-DJACK_HAS_METADATA_API', language : 'c')
endif

if cc.has_function('jack_set_port_rename_callback', prefix : '#include <jack/jack.h>')
	add_project_arguments('-DJACK_HAS_PORT_RENAME_CALLBACK', language : 'c')
endif

m_dep = cc.find_library('m')
rt_dep = cc.find_library('rt')
lv2_dep = dependency('lv2', version : '>=1.14.0')
lua_dep = dependency('lua', version : '>=5.3.0')
jack_dep = dependency('jack')
threads_dep = dependency('threads')

dsp_deps = [m_dep, rt_dep, lv2_dep, jack_dep, threads_dep, lua_dep]
ui_deps = [nk_pugl_dep]

varchunk_inc = include_directories('varchunk')
jackey_inc = include_directories('jackey')
osc_inc = include_directories('osc.lv2')
nk_pugl_inc = include_directories(join_paths('subprojects', 'nk_pugl'))

incs = [varchunk_inc, jackey_inc, osc_inc, nk_pugl_inc]

c_args = ['-fvisibility=hidden',
	'-ffast-math',
	'-Wno-unused-function']

dsp_srcs = [
	'patchmatrix.c',
	'patchmatrix_db.c',
	'patchmatrix_jack.c',
	'patchmatrix_nk.c'
]

cjson_lib = static_library('cJSON', join_paths('cJSON', 'cJSON.c'))

executable('patchmatrix', [dsp_srcs],
	c_args : c_args,
	dependencies : [dsp_deps, ui_deps],
	include_directories : incs,
	link_with : cjson_lib,
	install : true)

executable('patchmatrix_mixer', 'patchmatrix_mixer.c',
	c_args : c_args,
	dependencies : [dsp_deps, ui_deps],
	include_directories : incs,
	link_with : cjson_lib,
	install : true)

executable('patchmatrix_monitor', 'patchmatrix_monitor.c',
	c_args : c_args,
	dependencies : [dsp_deps, ui_deps],
	include_directories : incs,
	link_with : cjson_lib,
	install : true)

configure_file(
	input : 'patchmatrix.desktop.in',
	output : 'patchmatrix.desktop',
	configuration : conf_data,
	install_dir : appdir,
	install : true)

configure_file(
	input : cousine_regular_ttf,
	output : 'Cousine-Regular.ttf',
	copy : true,
	install_dir : pdatadir,
	install : true)

install_man('patchmatrix.1')
install_man('patchmatrix_mixer.1')
install_man('patchmatrix_monitor.1')

install_data(join_paths('pix', 'patchmatrix.png'),
	install_dir : join_paths(prefix, datadir, 'icons', 'hicolor', '256x256', 'apps'))

install_data(join_paths('pix', 'audio.png'),
	install_dir : pdatadir)
install_data(join_paths('pix', 'midi.png'),
	install_dir : pdatadir)
install_data(join_paths('pix', 'osc.png'),
	install_dir : pdatadir)
install_data(join_paths('pix', 'cv.png'),
	install_dir : pdatadir)
