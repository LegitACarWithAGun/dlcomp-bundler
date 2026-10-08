# https://stackoverflow.com/a/47801116
function(make_includable input_file output_file)
    file(READ ${input_file} content)
    set(delim "for_c++_include")
    set(content "R\"${delim}(${content})${delim}\"")
    file(WRITE ${output_file} "${content}")
endfunction(make_includable)

make_includable(${PATH}/src/template/bundle-template.luau ${PATH}/generated/bundle-template.luau)
