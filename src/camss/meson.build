udev_dep = dependency('libudev')

camss_files = files(
    '../file.cpp',
    '../graphics-wrapper.cpp',
    '../jpeg.cpp',
    '../media-device.cpp',
    '../pulse-recorder/pulse.cpp',
    '../record-context.cpp',
    '../udev.cpp',
    '../ui-v4l2.cpp',
    '../v4l2-encoder/encoder.cpp',
    '../v4l2.cpp',
    '../video-encoder/converter.cpp',
    '../video-encoder/encoder.cpp',
    '../window.cpp',
    '../yuv.cpp',
    'args.cpp',
    'camera.cpp',
    'main.cpp',
    'ui.cpp',
) + graphics_files + gawl_files

camss_deps = [udev_dep, tj_dep] + gawl_deps + video_encoder_deps + video_converter_deps + v4l2_encoder_deps + pulse_recorder_deps

executable(
    'wlcam-camss',
    camss_files,
    dependencies: camss_deps,
    install: true,
)

executable(
    'wlcam-camss-enumerator',
    files('../media-device.cpp', '../udev.cpp', 'enumerator.cpp'),
    dependencies: [udev_dep],
    install: true,
)
