project('showmethekey', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c11'])

cc = meson.get_compiler('c')

dep_wayland_client = dependency('wayland-client')
dep_wayland_protocols = dependency('wayland-protocols')
dep_cairo = dependency('cairo')
dep_pango = dependency('pango')
dep_pangocairo = dependency('pangocairo')
dep_libinput = dependency('libinput')
dep_libudev = dependency('libudev')
dep_xkbcommon = dependency('xkbcommon')

wayland_scanner = find_program('wayland-scanner')

wl_protocol_dir = dep_wayland_protocols.get_variable(pkgconfig : 'pkgdatadir')

protocols = [
  '/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml',
]

wl_protos_src = []
wl_protos_headers = []

foreach p : protocols
  xml = p
  
  wl_protos_src += custom_target(
    xml.underscorify() + '_c',
    input : xml,
    output : '@BASENAME@-protocol.c',
    command : [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  
  wl_protos_headers += custom_target(
    xml.underscorify() + '_client_h',
    input : xml,
    output : '@BASENAME@-client-protocol.h',
    command : [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
endforeach

executable('showmethekey',
  sources : ['src/main.c', 'src/input.c'] + wl_protos_src + wl_protos_headers,
  dependencies : [
    dep_wayland_client,
    dep_cairo,
    dep_pango,
    dep_pangocairo,
    dep_libinput,
    dep_libudev,
    dep_xkbcommon,
  ],
  install : true)
