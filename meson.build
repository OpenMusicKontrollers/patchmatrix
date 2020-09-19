project('nk_pugl', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=false',
	'b_lto=false',
	'c_std=gnu11'])

build_examples = get_option('build-examples')

static_link = false #meson.is_cross_build()

cc = meson.get_compiler('c')

m_dep = cc.find_library('m')
lv2_dep = dependency('lv2',
	version : '>=1.14.0')
glew_dep = dependency('glew',
	version : '>=2.1.0',
	static : static_link,
	required : false)
glu_dep = dependency('glu',
	version : '>=9.0.0',
	static : static_link,
	required : false)
if not glew_dep.found()
	glew_dep = declare_dependency(
		compile_args : '-DGLEW_STATIC',
		include_directories : include_directories('glew-2.1.0'),
		sources : join_paths('glew-2.1.0', 'glew.c'))
endif

deps = [m_dep, lv2_dep, glu_dep, glew_dep]
links = []

pugl_inc = include_directories('pugl')
inc_dir = [pugl_inc]

add_project_arguments('-D_GNU_SOURCE', language : 'c')

bin_srcs = [
	join_paths('example', 'example.c')
]

lib_srcs = [
	join_paths('pugl', 'pugl', 'detail', 'implementation.c')
]

c_args = ['-fvisibility=hidden',
	'-ffast-math']

if host_machine.system() == 'windows'
	deps += cc.find_library('opengl32')
	deps += cc.find_library('gdi32')
	deps += cc.find_library('ws2_32')
	lib_srcs += 'pugl/pugl/detail/win.c'
	lib_srcs += 'pugl/pugl/detail/win_gl.c'
elif host_machine.system() == 'darwin'
	add_languages('objc')
	links += ['-framework', 'OpenGL']
	links += ['-framework', 'Cocoa']
	lib_srcs += 'pugl/pugl/detail/mac.m'
	lib_srcs += 'pugl/pugl/detail/mac_gl.m'
else
	deps += dependency('gl')
	deps += dependency('x11', version : '>=1.6.0')
	deps += dependency('xext', version : '>=1.3.0')
	lib_srcs += 'pugl/pugl/detail/x11.c'
	lib_srcs += 'pugl/pugl/detail/x11_gl.c'
endif

nk_pugl_gl = declare_dependency(
	include_directories : inc_dir,
	dependencies : deps,
	link_args : links,
	sources : lib_srcs)

cousine_regular_ttf = configure_file(
	input : join_paths('nuklear', 'extra_font', 'Cousine-Regular.ttf'),
	output : 'Cousine-Regular.ttf',
	copy : true,
	install : false)

if build_examples

	executable('nk_pugl.gl', [bin_srcs],
		c_args : c_args,
		include_directories : inc_dir,
		dependencies: nk_pugl_gl,
		install : false)
endif
