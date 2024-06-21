project('wlcam', ['c', 'cpp'], version : '3.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])

graphics_files = files(
  'src/graphics/planar.cpp',
  'src/graphics/multitex.cpp',
  'src/graphics/yuv420sp.cpp',
  'src/graphics/yuv422i.cpp',
)

subdir('src/encoder')
subdir('src/gawl')

gawl_files = gawl_core_files + gawl_graphic_files + gawl_no_touch_callbacks_file
gawl_deps = gawl_core_deps + gawl_graphic_deps

subdir('src/uvc')

if get_option('ipu3')
  subdir('src/ipu3')
endif
