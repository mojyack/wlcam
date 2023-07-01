#deps = [
#  dependency('gawl', version : ['>=2.6.2', '<2.7']),
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

ipu3_src = graphics_src + files(
  'cio2.cpp',
  'main.cpp',
  'imgu.cpp',
  'args.cpp',
  '../file.cpp',
  '../jpeg.cpp',
  '../media-device.cpp',
  '../udev.cpp',
  '../v4l2.cpp',
  '../window.cpp',
  '../yuv.cpp',
)

executable('wlcam-ipu3', ipu3_src,
            dependencies : [gawl, dependency('libudev'), dependency('libturbojpeg')],
            install : true)

ipu3_enumerator_src = files(
  'enumerator.cpp',
  'cio2.cpp',
  'imgu.cpp',
  '../v4l2.cpp',
  '../udev.cpp',
  '../media-device.cpp',
)

executable('wlcam-ipu3-enumerator', ipu3_enumerator_src,
            dependencies : [dependency('libudev')],
            install : true)
