{
    'targets': [
      {
        'target_name': 'http_client',
        'type': 'executable',
        'defines': [
        ],
        'include_dirs': [
          '..',
          '../http_client',  # TODO(tomfinegan): remove this path
          '../third_party',
          '../third_party/boost/include',
        ],
        'sources': [
          'basictypes.h',
          'buffer_util.cc',
          'buffer_util.h',
          'debug_util.h',
          'file_reader.cc',
          'file_reader.h',
          'file_util.h',
          'http_client_base.h',
          'http_client_main.cc',
          'http_uploader.cc',
          'http_uploader.h',
          'webm_buffer_parser.cc',
          'webm_buffer_parser.h',
          'webm_encoder.cc',
          'webm_encoder.h',
        ],
        'conditions': [
          ['OS=="linux"', {
            'defines': [
            ],
            'include_dirs': [
            ],
          }],
          ['OS=="win"', {
            'copies': [
              {
                'destination': '<(PRODUCT_DIR)',
                'files': [
                  '../third_party/curl/win/x86/libcurl.dll',
                  '../third_party/curl/win/x86/libeay32.dll',
                  '../third_party/curl/win/x86/libidn-11.dll',
                  '../third_party/curl/win/x86/librtmp.dll',
                  '../third_party/curl/win/x86/libssh2.dll',
                  '../third_party/curl/win/x86/libssl32.dll',
                  '../third_party/zlib/win/x86/zlib1.dll',
                ],
              },
            ],
            'default_configuration': 'Debug',
            'configurations': {
              'Debug': {
                'msvs_settings': {
                  'VCCLCompilerTool': {
                    'Optimization': '0',
                    'PreprocessorDefinitions': [
                      '_DEBUG', 
                      'BOOST_NO_EXCEPTIONS',
                      'WIN32',
                    ],
                    'RuntimeLibrary': '1',
                  },
                },
              },
              'Release': {
                'msvs_settings': {
                  'VCCLCompilerTool': {
                    'Optimization': '2',
                    'PreprocessorDefinitions': [
                      'NDEBUG', 
                      'BOOST_NO_EXCEPTIONS',
                      'WIN32',
                    ],
                    'RuntimeLibrary': '0',
                  },
                },              
              },
            },
            'defines': [
            ],
            'msvs_disabled_warnings': [
              4995, 4996
            ],
            'msvs_settings': {
              #'OutputDirectory': '$(ProjectDir)..\..\exe\webmlive\$(ProjectName)\$(PlatformName)\$(ConfigurationName)',
              #'IntermediateDirectory': '$(ProjectDir)..\..\obj\webmlive\$(ProjectName)\$(PlatformName)\$(ConfigurationName)',
              'VCCLCompilerTool': {
                'DebugInformationFormat': '3',  # C7 compatible debug info
                'Detect64BitPortabilityProblems': 'false',
                'WarningLevel': '4',
                'WarnAsError': 'true',
              },
              'VCLinkerTool': {
                'SubSystem': '2',  # Set /SUBSYSTEM:WINDOWS
              },
            },
            'sources': [
              'win/build_config_win.cc',
              'win/file_reader_win.cc',
              'win/file_reader_win.h',
              'win/file_util_win.cc',
              'win/media_type_dshow.cc',
              'win/media_type_dshow.h',
              'win/webm_encoder_dshow.cc',
              'win/webm_encoder_dshow.h',
              'win/webm_guids.cc',
              'win/webm_guids.h',
            ],
          }, { # OS != "win",
            'defines': [
            ],
          }],
        ],
      },
    ],
  }