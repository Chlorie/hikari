function (add_example TGT)
    add_executable(${TGT} "${TGT}.cpp")
    target_set_output_dirs(${TGT})
    target_link_libraries(${TGT} PRIVATE project_options hikari::hikari)
    target_set_cxx_std(${TGT})
endfunction ()

add_example(playground)
add_example(hkr2ly)
