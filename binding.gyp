{
  'targets': [
    {
      'target_name': 'csound-api',
      'include_dirs': [
        '<!(node -e "require(\'nan\')")'
      ],
      'sources': [
        'src/csound-api.cpp'
      ],
      'conditions': [
        ['OS == "mac"', {
          'libraries': [
            '/usr/local/lib/libcsnd.6.0.dylib'
          ],
          # This is needed so that Boost can find the <atomic> header.
          'xcode_settings': {
            'OTHER_CPLUSPLUSFLAGS': [
              '-stdlib=libc++'
            ],
            'MACOSX_DEPLOYMENT_TARGET': '10.7'
          }
        }],
        ['OS == "linux"', {
          'libraries': [
            '/usr/local/lib/libcsound64.so'
          ]
        }]
      ]
    }
  ]
}
