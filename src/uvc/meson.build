uvc_files = files(
    '../file.cpp',
    '../graphics-wrapper.cpp',
    '../jpeg.cpp',
    '../pulse-recorder/pulse.cpp',
    '../record-context.cpp',
    '../v4l2.cpp',
    '../video-encoder/converter.cpp',
    '../video-encoder/encoder.cpp',
    '../window.cpp',
    '../yuv.cpp',
    'args.cpp',
    'camera.cpp',
    'main.cpp',
) + graphics_files + gawl_files

uvc_deps = [tj_dep] + gawl_deps + video_encoder_deps + video_converter_deps + pulse_recorder_deps

executable('wlcam-uvc', uvc_files, dependencies: uvc_deps, install: true)
