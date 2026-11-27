# Warnings are errors, except for host tools built on Windows (long is 32 bits there)
enumproc_werror = build_machine.system() == 'windows' ? [] : ['-Werror']

libenum_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: static_library(
        'libenum',
        sources: files('libenum.c'),
        c_args: [
            '-std=gnu17',
            '-O3',
            '-Wall',
            '-Wextra',
            '-Wpedantic',
            '-Wconversion',
            '-Wno-sign-conversion',
        ] + enumproc_werror,
        include_directories: include_directories('.'),
        native: true,
    ),
)

libexpr_dep = declare_dependency(
    include_directories: include_directories('.'),
    link_with: static_library(
        'libexpr',
        sources: files('libexpr.c'),
        c_args: [
            '-std=gnu17',
            '-O3',
            '-Wall',
            '-Wextra',
            '-Wpedantic',
            '-Wconversion',
            '-Wno-sign-conversion',
        ] + enumproc_werror,
        include_directories: include_directories('.'),
        native: true,
    ),
)

enumproc_exe = executable(
    'enumproc',
    sources: files('enumproc.c'),
    c_args: [
        '-std=gnu17',
        '-O3',
        '-Wall',
        '-Wextra',
        '-Wpedantic',
        '-Wconversion',
        '-Wno-sign-conversion',
    ] + enumproc_werror,
    dependencies: [ libenum_dep, libexpr_dep ],
    native: true,
)
