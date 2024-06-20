simple_files = files(
  'args.cpp',
  'camera.cpp',
  'main.cpp',
  '../file.cpp',
  '../remote-server.cpp',
  '../jpeg.cpp',
  '../v4l2.cpp',
  '../window.cpp',
  '../yuv.cpp',
) + graphics_files + gawl_files

simpel_deps = [dependency('libturbojpeg')] + gawl_deps

executable('wlcam', simple_files,
            dependencies : simpel_deps,
            install : true)
