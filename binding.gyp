{
  'targets': [
    {
      'target_name': 'csound-api',
      'include_dirs': [
        '<!(node -e "require(\'nan\')")'
      ],
      'sources': [
        'src/csound-api.cc'
      ],
      'conditions': [
        ['OS == "mac"', {
          # When creating an Xcode project, this lets Xcode find headers.
          'include_dirs': [
            '/usr/local/include'
          ],
          'libraries': [
            '/usr/local/lib/libcsnd6.6.0.dylib'
          ],
          # This is needed so Boost can find the <atomic> header.
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': [
              '-stdlib=libc++'
            ],
            'MACOSX_DEPLOYMENT_TARGET': '10.7'
          }
        }],
        ['OS == "linux"', {
          'libraries': [
            '-lcsound64'
          ]
        }],
        ['OS == "win"', {
          'defines': [
            # This is needed due to the issue described at
            # https://connect.microsoft.com/VisualStudio/feedback/details/1892487
            '_ENABLE_ATOMIC_ALIGNMENT_FIX',
            # Prevent min and max macros from being defined in windows.h.
            'NOMINMAX'
          ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              'ExceptionHandling': 1 # /EHsc
            }
          }
        }]
      ]
    }
  ]
}
