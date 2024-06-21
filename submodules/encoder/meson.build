project('app', 'cpp', version: '1.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])

subdir('src/encoder')
subdir('src/pulse')

src = files(
  'src/example.cpp',
  'src/encoder/encoder.cpp',
  'src/encoder/converter.cpp',
  'src/pulse/pulse.cpp',
)

deps = video_encoder_deps + video_converter_deps + pulse_recorder_deps

executable('example', src, 
  dependencies : deps,
  install : true)
