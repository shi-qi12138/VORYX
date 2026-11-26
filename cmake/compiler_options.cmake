# 警告规则只作用于自有目标，不改变第三方依赖的编译选项。
function(voryx_enable_warnings target)
    if(MSVC)
        # 调试信息嵌入对象文件，安装静态库时不依赖构建目录中的编译器 PDB。
        set_property(TARGET ${target} PROPERTY MSVC_DEBUG_INFORMATION_FORMAT
            "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()
