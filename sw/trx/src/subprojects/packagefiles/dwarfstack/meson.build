project(
  'dwarfstack',
  'c',
  default_options: [
    'c_std=c2x',
    'warning_level=2',
  ],
)

c_compiler = meson.get_compiler('c')
build_opts = [
  '-Wno-unused',
  '-DDWST_STATIC',
  '-DNO_DBGHELP',
  '-DLIBDWARF_STATIC',
]
add_project_arguments(build_opts, language: 'c')
dep_zlib = dependency('zlib')

sources = [
  'mgwhelp/dwarf_pe.c',
  'src/dwst-exception-dialog.c',
  'src/dwst-exception.c',
  'src/dwst-process.c',
  'src/dwst-location.c',
  'src/dwst-file.c',
  'libdwarf/dwarf_debugnames.c',
  'libdwarf/dwarf_dsc.c',
  'libdwarf/dwarf_alloc.c',
  'libdwarf/dwarf_loclists.c',
  'libdwarf/dwarf_macro5.c',
  'libdwarf/dwarf_harmless.c',
  'libdwarf/dwarf_locationop_read.c',
  'libdwarf/dwarf_gnu_index.c',
  'libdwarf/dwarf_rnglists.c',
  'libdwarf/dwarf_error.c',
  'libdwarf/dwarf_init_finish.c',
  'libdwarf/dwarf_abbrev.c',
  'libdwarf/dwarf_xu_index.c',
  'libdwarf/dwarf_names.c',
  'libdwarf/dwarf_str_offsets.c',
  'libdwarf/dwarf_tsearchhash.c',
  'libdwarf/dwarf_die_deliv.c',
  'libdwarf/dwarf_frame.c',
  'libdwarf/dwarf_query.c',
  'libdwarf/dwarf_global.c',
  'libdwarf/dwarf_loc.c',
  'libdwarf/dwarf_tied.c',
  'libdwarf/dwarf_util.c',
  'libdwarf/dwarf_form.c',
  'libdwarf/dwarf_groups.c',
  'libdwarf/dwarf_frame2.c',
  'libdwarf/dwarf_memcpy_swap.c',
  'libdwarf/dwarf_leb.c',
  'libdwarf/dwarf_debuglink.c',
  'libdwarf/dwarf_string.c',
  'libdwarf/dwarf_line.c',
  'libdwarf/dwarf_fission_to_cu.c',
  'libdwarf/dwarf_find_sigref.c',
  'libdwarf/dwarf_ranges.c',
]

dependencies = [
  dep_zlib,
]

libdwarfstack = static_library(
  'libdwarfstack',
  sources,
  dependencies: dependencies,
  include_directories: [
    'include/',
    'mgwhelp/',
    'src/',
    'libdwarf/',
  ]
)

dep_dwarfstack = declare_dependency(
  link_whole: libdwarfstack,
  include_directories: [
    include_directories('include', is_system: true)
  ]
)
