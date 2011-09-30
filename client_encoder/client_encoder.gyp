{
  'targets': [
    {
      'target_name': 'client_encoder',
      'type': 'executable',
      'defines': [
      ],
      'include_dirs': [
        '../',
        '../third_party',
        '../third_party/boost/include',
        '../third_party/libogg',
        '../third_party/libvorbis',
        '../third_party/libvpx/vpx',
        '../third_party/libyuv/include',
      ],
      'sources': [
        'audio_encoder.cc',
        'audio_encoder.h',
        'basictypes.h',
        'buffer_pool-inl.h',
        'buffer_pool.h',
        'buffer_util.cc',
        'buffer_util.h',
        'client_encoder_base.h',
        'client_encoder_main.cc',
        'data_sink.h',
        'http_uploader.cc',
        'http_uploader.h',
        'video_encoder.cc',
        'video_encoder.h',
        'vorbis_encoder.cc',
        'vorbis_encoder.h',
        'vpx_encoder.cc',
        'vpx_encoder.h',
        'webm_buffer_parser.cc',
        'webm_buffer_parser.h',
        'webm_encoder.cc',
        'webm_encoder.h',
        'webm_mux.cc',
        'webm_mux.h',
      ],
      'conditions': [
        ['OS=="linux"', {
          'defines': [
          ],
          'include_dirs': [
          ],
        }],
        ['OS=="win"', {
          'default_configuration': 'Debug',
          'dependencies': [
            'win/direct_show_base_classes.gyp:direct_show_base_classes',
          ],
          'msvs_configuration_attributes': {
            'OutputDirectory': '$(ProjectDir)../../exe/webmlive/$(ProjectName)/$(PlatformName)/$(ConfigurationName)/',
            'IntermediateDirectory': '$(ProjectDir)../../obj/webmlive/$(ProjectName)/$(PlatformName)/$(ConfigurationName)/',
          },
          'msbuild_configuration_attributes': {
            'OutputDirectory': '$(ProjectDir)../../exe/webmlive/$(ProjectName)/$(Platform)/$(Configuration)/',
            'IntermediateDirectory': '$(ProjectDir)../../obj/webmlive/$(ProjectName)/$(Platform)/$(Configuration)/',
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'Optimization': '0',
                  'PreprocessorDefinitions': [
                    '_DEBUG',
                    'CURL_STATICLIB',
                    'BOOST_NO_EXCEPTIONS',
                    'WIN32',
                  ],
                  'RuntimeLibrary': '1',
                },
                'VCLinkerTool': {
                  'AdditionalLibraryDirectories': '../third_party/boost/win/x86/debug',
                },
              },
            },
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'Optimization': '2',
                  'PreprocessorDefinitions': [
                    'BOOST_NO_EXCEPTIONS',
                    'NDEBUG',
                    'CURL_STATICLIB',
                    'WIN32',
                  ],
                  'RuntimeLibrary': '0',
                },
                'VCLinkerTool': {
                  'AdditionalLibraryDirectories': '../third_party/boost/win/x86/release',
                },
              },
            },
          },
          'msvs_disabled_warnings': [
            4244, 4995, 4996
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'DebugInformationFormat': '1',  # C7 compatible debug info
              'WarningLevel': '4',
              'WarnAsError': 'false',
            },
            'VCLinkerTool': {
              'AdditionalDependencies': ['advapi32.lib'],
              'SubSystem': '1',  # Set /SUBSYSTEM:CONSOLE
            },
          },
          'sources': [
            'win/audio_sink_filter.cc',
            'win/audio_sink_filter.h',
            'win/build_config_win.cc',
            'win/dshow_util.cc',
            'win/dshow_util.h',
            'win/media_source_dshow.cc',
            'win/media_source_dshow.h',
            'win/media_type_dshow.cc',
            'win/media_type_dshow.h',
            'win/video_sink_filter.cc',
            'win/video_sink_filter.h',
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
