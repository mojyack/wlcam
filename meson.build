project('wlcam', 'cpp', version : '2.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])

graphics_src = files(
  'src/graphics/planar.cpp',
  'src/graphics/yuv420sp.cpp',
  'src/graphics/yuv422i.cpp',
)

gawl = dependency('gawl', version : ['>=2.6.2', '<2.7'])

subdir('src/ipu3')
subdir('src/simple')
