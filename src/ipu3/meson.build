ipu3_src = graphics_src + files(
  'algorithm.cpp',
  'args.cpp',
  'cio2.cpp',
  'imgu.cpp',
  'main.cpp',
  'uapi.cpp',
  'params.cpp',
  '../file.cpp',
  '../jpeg.cpp',
  '../media-device.cpp',
  '../udev.cpp',
  '../v4l2.cpp',
  '../window.cpp',
  '../yuv.cpp',
)

executable('wlcam-ipu3', ipu3_src,
            dependencies : [gawl, dependency('gawl-fc'), dependency('libudev'), dependency('libturbojpeg')],
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
