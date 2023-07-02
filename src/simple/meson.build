simple_src = graphics_src + files(
  'args.cpp',
  'camera.cpp',
  'main.cpp',
  '../file.cpp',
  '../remote-server.cpp',
  '../jpeg.cpp',
  '../v4l2.cpp',
  '../window.cpp',
  '../yuv.cpp',
)

executable('wlcam', simple_src,
            dependencies : [gawl, dependency('libturbojpeg')],
            install : true)
