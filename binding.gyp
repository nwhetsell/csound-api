{
  'targets': [
    {
      'target_name': 'csound-api',
      'include_dirs': [
        '<!(node --eval "require(\'nan\')")'
      ],
      'sources': [
        'src/csound-api.cc'
      ],
      'conditions': [
        ['OS == "mac"', {
          'libraries': [
            '-lcsnd6'
          ]
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
