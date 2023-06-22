project('wlcam', 'cpp', version : '1.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])

graphics_src = files(
  'src/graphics/planar.cpp',
  'src/graphics/yuv420sp.cpp',
  'src/graphics/yuv422i.cpp',
)

gawl = dependency('gawl', version : ['>=2.6.2', '<2.7'])

subdir('src/ipu3')
#deps = [
#  ,
#  dependency('libturbojpeg'),
#]
#
#src = files(
#  'src/main.cpp',
#  'src/args.cpp',
#  'src/v4l2.cpp',
#  'src/jpeg.cpp',
#  'src/window.cpp',
#  'src/camera.cpp',
#  'src/yuv.cpp',
#  'src/file.cpp',
#  'src/remote-server.cpp',
#  'src/graphics/yuyv-planar-graphic.cpp',
#  'src/graphics/yuyv-interleaved-graphic.cpp',
#)
#
#executable('wlcam', src,
#            dependencies : deps,
#            install : true)
