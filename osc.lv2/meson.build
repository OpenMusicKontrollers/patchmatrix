project('osc.lv2', 'c', default_options : [
	'buildtype=release',
	'warning_level=3',
	'werror=true',
	'b_lto=false',
	'c_std=c11'])

version = run_command('cat', 'VERSION').stdout().strip()

add_project_arguments('-D_GNU_SOURCE', language : 'c')

conf_data = configuration_data()
cc = meson.get_compiler('c')

lv2_dep = dependency('lv2')
thread_dep = dependency('threads')
deps = [lv2_dep, thread_dep]

c_args = []

if host_machine.system() == 'windows'
	deps += cc.find_library('ws2_32')
	c_args += '-Wno-error=format'
	c_args += '-Wno-error=format-extra-args'
endif

osc_test = executable('osc_test',
	join_paths('test', 'osc_test.c'),
	c_args : c_args,
	dependencies : deps,
	install : false)

# FIXME start virautl serial pair before test
# socat -d -d pty,raw,echo=0 pty,raw,echo=0
test('Test', osc_test,
	timeout : 240)
