file(GLOB CMD_SOURCES  CONFIGURE_DEPENDS "*.cpp")
list(APPEND SPECIAL_TARGETS test install)
list(APPEND REQUIRED_TARGETS
    arp base64 basename cat chmod chown clear comm cp cut date dd df diff dirname dmesg du echo env expr false fgrep
    file find grep groups head host hostname id ifconfig kill killall ln logout ls mkdir mount mv nproc
    pgrep pidof ping pkill pmap ps readlink realpath reboot rm rmdir route seq shutdown sleep sort stat stty su tail test
    touch tr true umount uname uniq uptime w wc which whoami xargs yes less
)
list(APPEND RECOMMENDED_TARGETS
    adjtime aplay abench asctl bt checksum chres cksum copy fortune gunzip gzip init install keymap lsirq lsof lspci man mknod mktemp
    nc netstat notify ntpquery open passwd pls printf pro shot tar tt unzip wallpaper zip
)

# FIXME: Support specifying component dependencies for utilities (e.g. WebSocket for telws)

foreach(CMD_SRC ${CMD_SOURCES})
    get_filename_component(CMD_NAME ${CMD_SRC} NAME_WE)
    if (CMD_NAME IN_LIST SPECIAL_TARGETS)
        set(TARGET_NAME "${CMD_NAME}-bin")
    else()
        set(TARGET_NAME "${CMD_NAME}")
    endif()
    if(CMD_NAME IN_LIST REQUIRED_TARGETS)
        serenity_component(
            ${CMD_NAME}
            REQUIRED
            TARGETS ${TARGET_NAME}
        )
    elseif(CMD_NAME IN_LIST RECOMMENDED_TARGETS)
        serenity_component(
            ${CMD_NAME}
            RECOMMENDED
            TARGETS ${TARGET_NAME}
        )
    else()
        serenity_component(
            ${CMD_NAME}
            TARGETS ${TARGET_NAME}
        )
    endif()
    if (CMD_NAME IN_LIST SPECIAL_TARGETS)
        add_executable(${TARGET_NAME} ${CMD_SRC})
        target_link_libraries(${TARGET_NAME} LibCore LibMain)
        install(TARGETS ${TARGET_NAME} RUNTIME DESTINATION bin)
        install(CODE "file(RENAME ${CMAKE_INSTALL_PREFIX}/bin/${CMD_NAME}-bin ${CMAKE_INSTALL_PREFIX}/bin/${CMD_NAME})")
    else()
        add_executable(${CMD_NAME} ${CMD_SRC})
        set_target_properties(${CMD_NAME} PROPERTIES EXCLUDE_FROM_ALL TRUE)
        target_link_libraries(${CMD_NAME} LibCore)
        install(TARGETS ${CMD_NAME} RUNTIME DESTINATION bin OPTIONAL)
    endif()
endforeach()

if (ENABLE_JAKT)
    add_jakt_executable(hello-jakt
        MAIN_SOURCE hello-world.jakt
    )
    target_link_libraries(hello-jakt LibC)
    set_target_properties(hello-jakt PROPERTIES EXCLUDE_FROM_ALL TRUE)
    install(TARGETS hello-jakt RUNTIME DESTINATION bin OPTIONAL)
    serenity_component(
        hello-jakt
        RECOMMENDED
        TARGETS hello-jakt
    )
endif()

install(CODE "file(CREATE_LINK grep ${CMAKE_INSTALL_PREFIX}/bin/egrep SYMBOLIC)")
install(CODE "file(CREATE_LINK grep ${CMAKE_INSTALL_PREFIX}/bin/rgrep SYMBOLIC)")

target_link_libraries(abench LibAudio LibMain LibCore)
target_link_libraries(adjtime LibMain)
target_link_libraries(allocate LibMain)
target_link_libraries(aplay LibAudio LibMain)
target_link_libraries(arp LibMain)
target_link_libraries(asctl LibAudio LibMain)
target_link_libraries(base64 LibMain)
target_link_libraries(basename LibMain)
target_link_libraries(beep LibMain)
target_link_libraries(blockdev LibMain)
target_link_libraries(bt LibSymbolication LibMain)
target_link_libraries(cal LibMain)
target_link_libraries(cat LibMain)
target_link_libraries(checksum LibCrypto LibMain)
target_link_libraries(chgrp LibMain)
target_link_libraries(chmod LibMain)
target_link_libraries(chown LibMain)
target_link_libraries(chres LibGUI LibMain)
target_link_libraries(cksum LibCrypto LibMain)
target_link_libraries(clear LibMain)
target_link_libraries(cmp LibMain)
target_link_libraries(comm LibMain)
target_link_libraries(config LibConfig LibMain)
target_link_libraries(copy LibGUI LibMain)
target_link_libraries(cp LibMain)
target_link_libraries(cpp-lexer LibCpp LibMain)
target_link_libraries(cpp-parser LibCpp LibMain)
target_link_libraries(cpp-preprocessor LibCpp LibMain)
target_link_libraries(cut LibMain)
target_link_libraries(date LibMain)
target_link_libraries(dd LibMain)
target_link_libraries(df LibMain)
target_link_libraries(diff LibDiff LibMain)
target_link_libraries(dirname LibMain)
target_link_libraries(disasm LibX86 LibMain)
target_link_libraries(disk_benchmark LibMain)
target_link_libraries(dmesg LibMain)
target_link_libraries(du LibMain)
target_link_libraries(echo LibMain)
target_link_libraries(env LibMain)
target_link_libraries(errno LibMain)
target_link_libraries(expr LibRegex LibMain)
target_link_libraries(false LibMain)
target_link_libraries(fdtdump LibDeviceTree LibMain)
target_link_libraries(fgrep LibMain)
target_link_libraries(file LibGfx LibIPC LibCompress LibMain)
target_link_libraries(find LibMain)
target_link_libraries(flock LibMain)
target_link_libraries(fortune LibMain)
target_link_libraries(functrace LibDebug LibX86 LibMain)
target_link_libraries(gml-format LibGUI LibMain)
target_link_libraries(grep LibRegex LibMain)
target_link_libraries(gron LibMain)
target_link_libraries(groupadd LibMain)
target_link_libraries(groupdel LibMain)
target_link_libraries(groups LibMain)
target_link_libraries(gunzip LibCompress LibMain)
target_link_libraries(gzip LibCompress LibMain)
target_link_libraries(head LibMain)
target_link_libraries(headless-browser LibCore LibGemini LibGfx LibHTTP LibWeb LibWebSocket LibMain)
target_link_libraries(hexdump LibMain)
target_link_libraries(host LibMain)
target_link_libraries(hostname LibMain)
target_link_libraries(id LibMain)
target_link_libraries(ifconfig LibMain)
target_link_libraries(ini LibMain)
target_link_libraries(install-bin LibMain)
target_link_libraries(jp LibMain)
target_link_libraries(js LibJS LibLine LibMain LibTextCodec)
link_with_locale_data(js)
target_link_libraries(kcov-example LibMain)
target_link_libraries(keymap LibKeyboard LibMain)
target_link_libraries(kill LibMain)
target_link_libraries(killall LibCore LibMain)
target_link_libraries(less LibMain)
target_link_libraries(ln LibMain)
target_link_libraries(logout LibMain)
target_link_libraries(ls LibMain)
target_link_libraries(lscpu LibMain)
target_link_libraries(lsirq LibMain)
target_link_libraries(lsof LibMain)
target_link_libraries(lsblk LibMain)
target_link_libraries(lspci LibPCIDB LibMain)
target_link_libraries(lsusb LibUSBDB LibMain)
target_link_libraries(man LibMarkdown LibMain)
target_link_libraries(markdown-check LibMarkdown LibMain)
target_link_libraries(matroska LibMain LibVideo)
target_link_libraries(md LibMarkdown LibMain)
target_link_libraries(mkdir LibMain)
target_link_libraries(mkfifo LibMain)
target_link_libraries(mknod LibMain)
target_link_libraries(mktemp LibMain)
target_link_libraries(mv LibMain)
target_link_libraries(mount LibMain)
target_link_libraries(nc LibMain)
target_link_libraries(netstat LibMain)
target_link_libraries(nl LibMain)
target_link_libraries(notify LibGUI LibMain)
target_link_libraries(nproc LibMain)
target_link_libraries(ntpquery LibMain)
target_link_libraries(open LibDesktop LibMain)
target_link_libraries(passwd LibCrypt LibMain)
target_link_libraries(paste LibGUI LibCore LibMain)
target_link_libraries(pathchk LibMain)
target_link_libraries(pgrep LibRegex LibMain)
target_link_libraries(pidof LibMain)
target_link_libraries(ping LibMain)
target_link_libraries(pkill LibRegex LibMain)
target_link_libraries(pledge LibMain)
target_link_libraries(pls LibCrypt LibMain)
target_link_libraries(pmap LibMain)
target_link_libraries(pmemdump LibMain)
target_link_libraries(printf LibMain)
target_link_libraries(pro LibMain LibProtocol)
target_link_libraries(profile LibMain)
target_link_libraries(ps LibMain)
target_link_libraries(purge LibMain)
target_link_libraries(pwd LibMain)
target_link_libraries(readelf LibMain)
target_link_libraries(readlink LibMain)
target_link_libraries(realpath LibMain)
target_link_libraries(reboot LibMain)
target_link_libraries(rev LibMain)
target_link_libraries(rm LibMain)
target_link_libraries(rmdir LibMain)
target_link_libraries(route LibMain)
target_link_libraries(run-tests LibRegex LibCoredump LibMain)
target_link_libraries(seq LibMain)
target_link_libraries(shot LibGUI LibMain)
target_link_libraries(shuf LibMain)
target_link_libraries(shutdown LibMain)
target_link_libraries(sleep LibMain)
target_link_libraries(sort LibMain)
target_link_libraries(sql LibLine LibMain LibSQL LibIPC)
target_link_libraries(stat LibMain)
target_link_libraries(strace LibMain)
target_link_libraries(stty LibMain)
target_link_libraries(su LibCrypt LibMain)
target_link_libraries(sync LibMain)
target_link_libraries(syscall LibMain)
target_link_libraries(sysctl LibMain)
target_link_libraries(tac LibMain)
target_link_libraries(tail LibMain)
target_link_libraries(tar LibMain LibArchive LibCompress)
target_link_libraries(tee LibMain)
target_link_libraries(telws LibProtocol LibLine LibMain)
target_link_libraries(test-bindtodevice LibMain)
target_link_libraries(test-fuzz LibCore LibGemini LibGfx LibHTTP LibIPC LibJS LibMarkdown LibShell LibMain)
target_link_libraries(test-imap LibIMAP LibMain)
target_link_libraries(test-pthread LibThreading LibMain)
target_link_libraries(test-unveil LibMain)
target_link_libraries(test_env LibMain)
target_link_libraries(timezone LibMain)
target_link_libraries(top LibMain)
target_link_libraries(touch LibMain)
target_link_libraries(tr LibMain)
target_link_libraries(traceroute LibMain)
target_link_libraries(tree LibMain)
target_link_libraries(true LibMain)
target_link_libraries(truncate LibMain)
target_link_libraries(tt LibMain)
target_link_libraries(tty LibMain)
target_link_libraries(umount LibMain)
target_link_libraries(uname LibMain)
target_link_libraries(uniq LibMain)
target_link_libraries(unzip LibArchive LibCompress LibMain)
target_link_libraries(update-cpp-test-results LibCpp LibCore LibMain)
target_link_libraries(uptime LibMain)
target_link_libraries(useradd LibMain)
target_link_libraries(userdel LibMain)
target_link_libraries(usermod LibMain)
target_link_libraries(utmpupdate LibMain)
target_link_libraries(w LibMain)
target_link_libraries(wallpaper LibGUI LibMain)
target_link_libraries(wasm LibMain LibWasm LibLine)
target_link_libraries(watch LibMain)
target_link_libraries(wc LibMain)
target_link_libraries(which LibMain)
target_link_libraries(whoami LibMain)
target_link_libraries(wsctl LibGUI LibMain)
target_link_libraries(xargs LibMain)
target_link_libraries(xml LibMain LibXML)
target_link_libraries(yes LibMain)
target_link_libraries(zip LibArchive LibCompress LibCrypto LibMain)
