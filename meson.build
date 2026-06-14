project(
    'wlcam',
    ['c', 'cpp'],
    version: '3.0.0',
    default_options: ['warning_level=3', 'werror=false', 'cpp_std=c++23'],
)

add_project_arguments('-Wno-missing-field-initializers', language: 'cpp')

graphics_files = files(
    'src/graphics/bayer.cpp',
    'src/graphics/multitex.cpp',
    'src/graphics/planar.cpp',
    'src/graphics/yuv420sp.cpp',
    'src/graphics/yuv422i.cpp',
)

subdir('src/video-encoder')
subdir('src/v4l2-encoder')
subdir('src/pulse-recorder')
subdir('src/gawl')

gawl_files = gawl_core_files + gawl_graphic_files + gawl_textrender_files + gawl_fc_files + gawl_no_touch_callbacks_file + gawl_empty_texture_files
gawl_deps = gawl_core_deps + gawl_graphic_deps + gawl_textrender_deps + gawl_fc_deps

tj_dep = dependency('libturbojpeg')

if get_option('uvc')
    subdir('src/uvc')
endif

if get_option('ipu3')
    subdir('src/ipu3')
endif

if get_option('camss')
    subdir('src/camss')
endif

if get_option('tests')
    executable(
        'video-encoder-example',
        files(
            'src/pulse-recorder/pulse.cpp',
            'src/video-encoder-example.cpp',
            'src/video-encoder/converter.cpp',
            'src/video-encoder/encoder.cpp',
        ),
        dependencies: video_encoder_deps + video_converter_deps + pulse_recorder_deps,
    )
endif
