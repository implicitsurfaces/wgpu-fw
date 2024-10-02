import itertools
import platform

import os

AddOption(
    '--wasm',
    action='store_true',
    help='Compile to WASM',
    default=False)
AddOption(
    '--sanitize',
    action='store_true',
    help='Enable address sanitization',
    default=False)
AddOption(
    '--opencv',
    action='store_true',
    help='Link with OpenCV',
    default=False)

debug  = ARGUMENTS.get('debug',  False)
noopt  = ARGUMENTS.get('noopt',  False)
opencv = GetOption('opencv')

Ts = Builder(
    action='tsc $TS_OPTS $SOURCE --out $TARGET',
    suffix='.js',
    src_suffix='.ts')


def subst_wasm_flags(env, wasm_flags):
    wasm_opts = (('-s', f'{k}={v}') for k,v in wasm_flags.items())
    return list(itertools.chain(*wasm_opts))

wasm_cflags = {
    'NO_DISABLE_EXCEPTION_CATCHING' : 1,
    'USE_LIBPNG'                    : 1,
}

wasm_linkflags = {
    'FULL_ES3'            : 1,
    'USE_GLFW'            : 3,
    'ALLOW_MEMORY_GROWTH' : 1,
}

pkg_config_path = os.environ.get('PKG_CONFIG_PATH') if 'PKG_CONFIG_PATH' in os.environ else []

if debug:
    wasm_cflags['ASSERTIONS'] = 1

env = Environment(
    CXX='clang++',
    CXXFLAGS=[
        '-O3' if not noopt else '-O0',
        '-std=c++20',
        '-fcolor-diagnostics',
        '-Werror',
        '-Wno-return-type-c-linkage',
        '-Wno-limited-postlink-optimizations',
        '-Wno-unknown-warning-option',
        # '-v'
    ],
    LIBS=['geomc', 'wgpu_native', 'glfw3webgpu'],
    CPPPATH=['#'],
    TS_OPTS=['--target', 'ES2018'],
    BUILDERS={'Ts' : Ts},
    ENV={'PKG_CONFIG_PATH': pkg_config_path},
    COMPILATIONDB_USE_ABSPATH=True,
)
env.AddMethod(subst_wasm_flags)

env.Tool('compilation_db')

if debug:
    env.Append(CXXFLAGS='-g')

if GetOption('sanitize'):
    env.Append(CXXFLAGS =['-fsanitize=address', '-fno-omit-frame-pointer'])
    env.Append(LINKFLAGS=['-fsanitize=address', '-fno-omit-frame-pointer'])

if GetOption('wasm'):
    arch           = 'wasm'
    env['CXX']     = 'em++'
    env['AR']      = 'emar'
    env['RANLIB']  = 'emranlib'
    # todo: make this more portable.
    env.Append(LIBPATH=['/usr/local/wasm/lib'])
    env.Append(CPPPATH=['/usr/local/wasm/include'])
    env.Append(CXXFLAGS=env.subst_wasm_flags(wasm_cflags))
    env.Append(LINKFLAGS=env.subst_wasm_flags(wasm_linkflags))
    env.Append(LIBS=['glfw3', 'embind'])
    if debug:
        env.Append(LINKFLAGS=['-gsource-map'])
else:
    arch = 'native'
    # rpath = '/usr/local/lib'
    # env.Append(LINKFLAGS=[f'-Wl,-rpath,{rpath}'])
    env.Append(CXXFLAGS='-march=native')
    env.Append(LIBPATH=['/usr/local/lib'])
    # if platform.system().lower() == 'darwin':
    #     env.Append(LINKFLAGS=['-framework', 'OpenGL'])
    extra_libs = ['glfw3', 'libpng']
    if opencv:
        extra_libs.append('opencv4')
        env.Append(CXXFLAGS=[
            # opencv has warnings for auto-casting of enums.
            # see https://github.com/opencv/opencv/issues/20269
            '-Wno-deprecated-anon-enum-enum-conversion'
        ])
    for lib in extra_libs:
        env.ParseConfig(f'pkg-config --cflags --libs --static {lib}')
    if env['PLATFORM'].lower() == 'darwin':
        env.Append(LINKFLAGS=[
            # '-framework', 'Cocoa',
            # '-framework', 'IOKit',
            # '-framework', 'CoreVideo',
            '-framework', 'QuartzCore',
        ])

env['ARCH'] = arch

Export("env")

# data = SConscript('objects/SConscript', variant_dir='build/objects')
prog = SConscript('stereo/SConscript',  variant_dir=f'build/{arch}/stereo')
test = SConscript('test/SConscript',    variant_dir=f'build/{arch}/test')

comp_db = env.CompilationDatabase(target='compile_commands.json')

Default([prog, comp_db])
