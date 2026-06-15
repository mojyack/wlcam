udev_dep = dependency('libudev')

ipu3_files = files(
    '../file.cpp',
    '../graphics-wrapper.cpp',
    '../jpeg.cpp',
    '../media-device.cpp',
    '../pulse-recorder/pulse.cpp',
    '../record-context.cpp',
    '../udev.cpp',
    '../v4l2.cpp',
    '../video-encoder/converter.cpp',
    '../video-encoder/encoder.cpp',
    '../window.cpp',
    '../yuv.cpp',
    'algorithm.cpp',
    'args.cpp',
    'cio2.cpp',
    'imgu.cpp',
    'main.cpp',
    'params.cpp',
    'uapi.cpp',
) + graphics_files + gawl_files + gawl_textrender_files + gawl_fc_files

ipu3_deps = [udev_dep, tj_dep] + gawl_deps + video_encoder_deps + video_converter_deps + pulse_recorder_deps

ipu3_enumerator_files = files(
    '../media-device.cpp',
    '../udev.cpp',
    '../v4l2.cpp',
    'cio2.cpp',
    'enumerator.cpp',
    'imgu.cpp',
)

ipu3_enumerator_deps = [udev_dep]

executable('wlcam-ipu3', ipu3_files, dependencies: ipu3_deps, install: true)

executable(
    'wlcam-ipu3-enumerator',
    ipu3_enumerator_files,
    dependencies: ipu3_enumerator_deps,
    install: true,
)
