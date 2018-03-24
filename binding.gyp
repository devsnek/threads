{
  'target_defaults': {
    'default_configuration': 'Release',
    'configurations': {
      'Release': {
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '3',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'NO',
        },
         'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': 3,
            'FavorSizeOrSpeed': 1,
          },
        },
      },
      'Debug': {
        'xcode_settings': {
          'GCC_OPTIMIZATION_LEVEL': '0',
          'GCC_GENERATE_DEBUGGING_SYMBOLS': 'YES',
        },
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': 0,
            'FavorSizeOrSpeed': 0,
          },
        },
      },
    },
  },
  'targets': [
    {
      'target_name': 'threads',
      'cflags_cc': [ '-std=c++14' ],
      'cflags_cc!': [ '-fno-exceptions', '-fno-rtti' ],
      'xcode_settings': {
        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
        'GCC_ENABLE_CPP_RTTI': 'YES',
        'CLANG_CXX_LANGUAGE_STANDARD': 'c++14',
      },
      'msvs_settings': {
        'VCCLCompilerTool': {
          'ExceptionHandling': '1',
          'RuntimeTypeInfo': 'true',
          'AdditionalOptions': [ '/GR' ],
        },
      },
      'msvs_disabled_warnings': [ 4068 ], # Unknown pragma
      'conditions': [
        [ 'OS == "win"',
          { 'defines': [ 'NOMINMAX' ] },
        ],
      ],
      'sources': [
        'src/NativeUtil.cc',
        'src/Worker.cc',
        'src/threads.cc',
      ],
    },
  ],
}
