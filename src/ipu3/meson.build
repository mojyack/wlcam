udev_dep = dependency('libudev')

ipu3_files = files(
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
  '../graphics-wrapper.cpp',
  '../vcw/window.cpp',
  '../record-context.cpp',
  '../encoder/encoder.cpp',
  '../encoder/converter.cpp',
  '../pulse/pulse.cpp',
) + graphics_files + gawl_files + gawl_textrender_files + gawl_fc_files

ipu3_deps = [udev_dep, tj_dep] + gawl_deps + video_encoder_deps + video_converter_deps + pulse_recorder_deps

ipu3_enumerator_files = files(
  'enumerator.cpp',
  'cio2.cpp',
  'imgu.cpp',
  '../v4l2.cpp',
  '../udev.cpp',
  '../media-device.cpp',
)

ipu3_enumerator_deps = [udev_dep]

executable('wlcam-ipu3', ipu3_files,
            dependencies : ipu3_deps,
            install : true)

executable('wlcam-ipu3-enumerator', ipu3_enumerator_files,
            dependencies : ipu3_enumerator_deps,
            install : true)
