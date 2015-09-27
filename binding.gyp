{
  'targets': [
    {
      'target_name': 'csound-api',
      'sources': [
        'src/csound-api.cpp'
      ],
      'include_dirs': [
        '/Library/Frameworks/CsoundLib64.framework/Headers',
        '<!(node -e "require(\'nan\')")'
      ],
      'libraries': [
        '/Library/Frameworks/CsoundLib64.framework/Versions/Current/libcsnd6.6.0.dylib'
      ]
    }
  ]
}
