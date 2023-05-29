project('wlcam', 'cpp', version : '1.0.0', default_options : ['warning_level=3', 'werror=false', 'cpp_std=c++20'])

deps = [
  dependency('gawl', version : ['>=2.6.2', '<2.7']),
  dependency('libturbojpeg'),
]

src = files(
  'src/main.cpp',
  'src/args.cpp',
  'src/v4l2.cpp',
  'src/jpeg.cpp',
  'src/window.cpp',
  'src/camera.cpp',
  'src/yuv.cpp',
  'src/file.cpp',
  'src/remote-server.cpp',
  'src/graphics/yuyv-planar-graphic.cpp',
  'src/graphics/yuyv-interleaved-graphic.cpp',
)

executable('wlcam', src,
            dependencies : deps,
            install : true)
