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
          'include_dirs': [
            '/Library/Frameworks/CsoundLib64.framework/Headers'
          ],
          'libraries': [
            '/Library/Frameworks/CsoundLib64.framework/Versions/Current/libcsnd6.6.0.dylib'
          ]
        }],
        ['OS == "linux"', {
          'include_dirs': [
            '/usr/include/csound'
          ],
          'libraries': [
            '/usr/lib/libcsound64.so'
          ]
        }]
      ]
    }
  ]
}
