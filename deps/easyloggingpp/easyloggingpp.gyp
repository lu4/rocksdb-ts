{'targets': [{
    # 'variables': {
    #     'conditions': [
    #         ['OS=="linux"',   {'os_include': 'linux'}]
    #       , ['OS=="mac"',     {'os_include': 'mac'}]
    #       , ['OS=="solaris"', {'os_include': 'solaris'}]
    #       , ['OS=="win"',     {'os_include': 'win32'}]
    #       , ['OS=="freebsd"', {'os_include': 'freebsd'}]
    #       , ['OS=="openbsd"', {'os_include': 'openbsd'}]
    #     ]
    # }
    # Overcomes an issue with the linker and thin .a files on SmartOS
    'target_name': 'easyloggingpp',
    'type': 'static_library',

    'cflags': ['-pthread'],
    'cflags_cc': ['-pthread'],
    'link_settings': {
        'libraries': ['-lpthread']
    },

    'standalone_static_library': 1,
    'defines': [
        'ELPP_THREAD_SAFE=1',
        'ELPP_FEATURE_PERFORMANCE_TRACKING=1',
        'ELPP_PERFORMANCE_MICROSECONDS=1'
    ],
    'include_dirs': [
        'easyloggingpp/src'
    ],
    'direct_dependent_settings': {
        'include_dirs': [
            'easyloggingpp/src'
        ]
    },
    'defines': [
        # 'HAVE_CONFIG_H=1'
    ],
    'conditions': [
        ['OS == "win"', {
        }], ['OS == "linux"', {
            # TODO: As per https://github.com/zuhd-org/easyloggingpp#multi-threading an "-lpthread" option should be specified for linux

            'cflags!': ['-fno-exceptions'],
            'cflags_cc!': ['-fno-exceptions']
        }], ['OS == "freebsd"', {
            # 'cflags': [
            #     '-Wno-sign-compare', '-Wno-unused-function'
            # ]
        }], ['OS == "openbsd"', {
            # 'cflags': [
            #     '-Wno-sign-compare', '-Wno-unused-function'
            # ]
        }], ['OS == "solaris"', {
            # 'cflags': [
            #     '-Wno-sign-compare', '-Wno-unused-function'
            # ]
        }], ['OS == "mac"', {
            'xcode_settings': {
                'OTHER_CPLUSPLUSFLAGS': [
                    '-mmacosx-version-min=10.8'
                ],
                'GCC_ENABLE_CPP_EXCEPTIONS': 'YES'
            }
        }]
    ], 'sources': [
        'easyloggingpp/src/easylogging++.h',
        'easyloggingpp/src/easylogging++.cc'
    ]
}]}
