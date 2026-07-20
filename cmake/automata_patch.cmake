# automata_add_patch(<name> <sources...>): a hot-reloadable patch module the
# host watches. Must be built by the same toolchain and flags as the host
# (ADR 0001) — building it from the same tree guarantees that.
function(automata_add_patch NAME)
  add_library(${NAME} MODULE ${ARGN})
  target_link_libraries(${NAME} PRIVATE automata::automata)
  target_compile_options(${NAME} PRIVATE ${AUTOMATA_WARNINGS})
  # A predictable file name on every platform: <name>.dll / <name>.so
  set_target_properties(${NAME} PROPERTIES PREFIX "")
endfunction()
