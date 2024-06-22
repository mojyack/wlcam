
uvc_files = files(
  'args.cpp',
  'camera.cpp',
  'main.cpp',
  '../file.cpp',
  '../jpeg.cpp',
  '../v4l2.cpp',
  '../window.cpp',
  '../yuv.cpp',
  '../graphics-wrapper.cpp',
  '../args.cpp',
  '../encoder/encoder.cpp',
  '../encoder/converter.cpp',
  '../pulse/pulse.cpp',
) + graphics_files + gawl_files

uvc_deps = [dependency('libturbojpeg')] + gawl_deps + video_encoder_deps + video_converter_deps + pulse_recorder_deps

executable('wlcam-uvc', uvc_files,
            dependencies : uvc_deps,
            install : true)
