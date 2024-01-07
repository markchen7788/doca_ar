
project('DOCA_SAMPLE', 'C', 'CPP',
	# Get version number from file.
	version: run_command(find_program('cat'),
		files('/opt/mellanox/doca/applications/VERSION'), check: true).stdout().strip(),
	license: 'Proprietary',
	default_options: ['buildtype=debug'],
	meson_version: '>= 0.61.2'
)

SAMPLE_NAME = 'doca_ar'

# Comment this line to restore warnings of experimental DOCA features
add_project_arguments('-D DOCA_ALLOW_EXPERIMENTAL_API', language: ['c', 'cpp'])

sample_dependencies = []
# Required for all DOCA programs
sample_dependencies += dependency('doca-common')
# Library needed for this sample
sample_dependencies += dependency('doca-flow')
# Library used by the main() function
sample_dependencies += dependency('doca-argp')
# 3rd Party dependencies
sample_dependencies += dependency('libdpdk')

path='src/'
sample_srcs = [
	# The sample itself
	path+SAMPLE_NAME + '_env.c',
	path+SAMPLE_NAME + '_pipe.c',
	path+SAMPLE_NAME + '_conntrack.c',
	path+SAMPLE_NAME + '_core.c',
	# Main function for the sample's executable
	path+'doca_ar.c',
	# Common code for the DOCA library samples
	# Common code for all DOCA applications
	'./src/common/src/dpdk_utils.c',
	'./src/common/src/offload_rules.c',
]

sample_inc_dirs  = []
# Common DOCA library logic
sample_inc_dirs += include_directories('.')
# Common DOCA logic (applications)
sample_inc_dirs += include_directories('./src/common/src')

executable(SAMPLE_NAME, sample_srcs,
	dependencies : sample_dependencies,
	include_directories: sample_inc_dirs,
	install: false)
