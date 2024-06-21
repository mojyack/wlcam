
uvc_files = files(
  'args.cpp',
  'camera.cpp',
  'main.cpp',
  '../file.cpp',
  '../remote-server.cpp',
  '../jpeg.cpp',
  '../v4l2.cpp',
  '../window.cpp',
  '../yuv.cpp',
  '../graphics-wrapper.cpp',
  '../encoder/encoder.cpp',
) + graphics_files + gawl_files

uvc_deps = [dependency('libturbojpeg')] + gawl_deps + video_encoder_deps

executable('wlcam-uvc', uvc_files,
            dependencies : uvc_deps,
            install : true)
