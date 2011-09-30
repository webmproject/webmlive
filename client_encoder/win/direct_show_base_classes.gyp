# Shamelessly copied from WebRTC.
{
  'targets': [
    {
      'target_name': 'direct_show_base_classes',
      'type': 'static_library',
      'variables': {
        # Path needed to build the Direct Show base classes on Windows. The
        # code is included in the Windows SDK.
        'direct_show_dir':
          'C:/Program Files/Microsoft SDKs/Windows/v7.1/Samples/multimedia/directshow/baseclasses/',
      },
      'defines!': [
        'NOMINMAX',
      ],
      'include_dirs': [
        '<(direct_show_dir)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(direct_show_dir)',
          '<(direct_show_dir)/..',
        ],
      },
      'sources': [
        '<(direct_show_dir)amextra.cpp',
        '<(direct_show_dir)amextra.h',
        '<(direct_show_dir)amfilter.cpp',
        '<(direct_show_dir)amfilter.h',
        '<(direct_show_dir)amvideo.cpp',
        '<(direct_show_dir)arithutil.cpp',
        '<(direct_show_dir)cache.h',
        '<(direct_show_dir)checkbmi.h',
        '<(direct_show_dir)combase.cpp',
        '<(direct_show_dir)combase.h',
        '<(direct_show_dir)cprop.cpp',
        '<(direct_show_dir)cprop.h',
        '<(direct_show_dir)ctlutil.cpp',
        '<(direct_show_dir)ctlutil.h',
        '<(direct_show_dir)ddmm.cpp',
        '<(direct_show_dir)ddmm.h',
        '<(direct_show_dir)dllentry.cpp',
        '<(direct_show_dir)dllsetup.cpp',
        '<(direct_show_dir)dllsetup.h',
        '<(direct_show_dir)dxmperf.h',
        '<(direct_show_dir)fourcc.h',
        '<(direct_show_dir)measure.h',
        '<(direct_show_dir)msgthrd.h',
        '<(direct_show_dir)mtype.cpp',
        '<(direct_show_dir)mtype.h',
        '<(direct_show_dir)outputq.cpp',
        '<(direct_show_dir)outputq.h',
        '<(direct_show_dir)perflog.cpp',
        '<(direct_show_dir)perflog.h',
        '<(direct_show_dir)perfstruct.h',
        '<(direct_show_dir)pstream.cpp',
        '<(direct_show_dir)pstream.h',
        '<(direct_show_dir)pullpin.cpp',
        '<(direct_show_dir)pullpin.h',
        '<(direct_show_dir)refclock.cpp',
        '<(direct_show_dir)refclock.h',
        '<(direct_show_dir)reftime.h',
        '<(direct_show_dir)renbase.cpp',
        '<(direct_show_dir)renbase.h',
        '<(direct_show_dir)schedule.cpp',
        '<(direct_show_dir)schedule.h',
        '<(direct_show_dir)seekpt.cpp',
        '<(direct_show_dir)seekpt.h',
        '<(direct_show_dir)source.cpp',
        '<(direct_show_dir)source.h',
        '<(direct_show_dir)streams.h',
        '<(direct_show_dir)strmctl.cpp',
        '<(direct_show_dir)strmctl.h',
        '<(direct_show_dir)sysclock.cpp',
        '<(direct_show_dir)sysclock.h',
        '<(direct_show_dir)transfrm.cpp',
        '<(direct_show_dir)transfrm.h',
        '<(direct_show_dir)transip.cpp',
        '<(direct_show_dir)transip.h',
        '<(direct_show_dir)videoctl.cpp',
        '<(direct_show_dir)videoctl.h',
        '<(direct_show_dir)vtrans.cpp',
        '<(direct_show_dir)vtrans.h',
        '<(direct_show_dir)winctrl.cpp',
        '<(direct_show_dir)winctrl.h',
        '<(direct_show_dir)winutil.cpp',
        '<(direct_show_dir)winutil.h',
        '<(direct_show_dir)wxdebug.cpp',
        '<(direct_show_dir)wxdebug.h',
        '<(direct_show_dir)wxlist.cpp',
        '<(direct_show_dir)wxlist.h',
        '<(direct_show_dir)wxutil.cpp',
        '<(direct_show_dir)wxutil.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'msvs_configuration_attributes': {
            'OutputDirectory': '$(ProjectDir)../../../exe/webmlive/$(ProjectName)/$(PlatformName)/$(ConfigurationName)/',
            'IntermediateDirectory': '$(ProjectDir)../../../obj/webmlive/$(ProjectName)/$(PlatformName)/$(ConfigurationName)/',
          },
          'msbuild_configuration_attributes': {
            'OutputDirectory': '$(ProjectDir)../../../exe/webmlive/$(ProjectName)/$(Platform)/$(Configuration)/',
            'IntermediateDirectory': '$(ProjectDir)../../../obj/webmlive/$(ProjectName)/$(Platform)/$(Configuration)/',
          },
          'default_configuration': 'Debug',
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'Optimization': '0',
                  'PreprocessorDefinitions': [
                    '_DEBUG',
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
                    'WIN32',
                  ],
                  'RuntimeLibrary': '0',
                },
              },
            },
          },
        }],
      ],
    },
  ],
}


