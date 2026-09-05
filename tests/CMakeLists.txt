# Biblia Elim - pruebas
#
# No entran en la compilación normal: se piden a mano.
#
#   cmake --build build --target pulpito_test && ./build/tests/pulpito_test

add_executable (pulpito_test
  EXCLUDE_FROM_ALL
  pulpito_test.c
  ${PROJECT_SOURCE_DIR}/src/main/pulpito_pasos.c
  )

target_link_libraries (pulpito_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (morfologia_test
  EXCLUDE_FROM_ALL
  morfologia_test.c
  ${PROJECT_SOURCE_DIR}/src/main/morfologia.c
  )

target_link_libraries (morfologia_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (glosa_test
  EXCLUDE_FROM_ALL
  glosa_test.c
  ${PROJECT_SOURCE_DIR}/src/main/glosa.c
  )

target_link_libraries (glosa_test
  PRIVATE
  PkgConfig::Core
  )

add_executable (testimonios_test
  EXCLUDE_FROM_ALL
  testimonios_test.c
  ${PROJECT_SOURCE_DIR}/src/main/testimonios.c
  )

target_compile_definitions (testimonios_test
  PRIVATE
  SRCDIR="${PROJECT_SOURCE_DIR}"
  )

target_include_directories (testimonios_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Gnome
  )

target_link_libraries (testimonios_test
  PRIVATE
  PkgConfig::Core
  PkgConfig::Gnome
  )

add_executable (buscar_notas_test
  EXCLUDE_FROM_ALL
  buscar_notas_test.c
  ${PROJECT_SOURCE_DIR}/src/main/buscar_notas.c
  ${PROJECT_SOURCE_DIR}/src/main/busqueda_tildes.c
  )

target_link_libraries (buscar_notas_test
  PRIVATE
  PkgConfig::Core
  )
