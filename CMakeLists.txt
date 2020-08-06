cmake_minimum_required(VERSION 3.8)
project(mc)

#set(CMAKE_C_STANDARD 11)
#set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})
link_directories(${GLIB2_LIBRARY_DIRS})
#add_executable (gtk-test main.c)
add_definitions(${GLIB2_CFLAGS_OTHER})
#target_link_libraries (gtk-test ${GTK2_LIBRARIES})

pkg_check_modules(EXT2FS glib-2.0)

#FIXME hardcoded paths
add_definitions(-DLIBEXECDIR="/usr/lib/mc/")
add_definitions(-DSAVERDIR="/usr/lib/mc/")
add_definitions(-DCONFIGDIR="/etc/mc/")
add_definitions(-DEXTHELPERSDIR="/usr/lib/mc/ext.d/")
add_definitions(-DSYSCONFDIR="/etc/mc/")
add_definitions(-DDATADIR="/usr/share/mc/")
add_definitions(-DLOCALEDIR="/usr/share/locale/")

add_definitions(-DHAVE_STDARG_H)
add_definitions(-DHAVE_GETTIMEOFDAY_TZ)

include_directories(.)
include_directories(lib)
include_directories(lib/event)
include_directories(lib/filehighlight)
include_directories(lib/search)
include_directories(lib/skin)
include_directories(lib/tty)
include_directories(lib/vfs)
include_directories(lib/widget)
include_directories(maint/templates)
include_directories(src)
include_directories(src/consaver)
include_directories(src/diffviewer)
include_directories(src/editor)
include_directories(src/filemanager)
include_directories(src/subshell)
include_directories(src/vfs)
include_directories(src/vfs/cpio)
include_directories(src/vfs/extfs)
include_directories(src/vfs/fish)
include_directories(src/vfs/ftpfs)
include_directories(src/vfs/local)
include_directories(src/vfs/sfs)
include_directories(src/vfs/sftpfs)
include_directories(src/vfs/smbfs)
include_directories(src/vfs/smbfs/helpers/include)
include_directories(src/vfs/tar)
include_directories(src/vfs/undelfs)
include_directories(src/viewer)
include_directories(tests)

add_executable(mc
        lib/event/event.cpp
        lib/event/manage.cpp
        lib/event/raise.cpp
        lib/filehighlight/common.cpp
        lib/filehighlight/get-color.cpp
        lib/filehighlight/ini-file-read.cpp
        lib/mcconfig/common.cpp
        lib/mcconfig/get.cpp
        lib/mcconfig/history.cpp
        lib/mcconfig/paths.cpp
        lib/mcconfig/set.cpp
        lib/search/glob.cpp
        lib/search/hex.cpp
        lib/search/lib.cpp
        lib/search/normal.cpp
        lib/search/regex.cpp
        lib/search/search.cpp
        lib/skin/colors-old.cpp
        lib/skin/colors.cpp
        lib/skin/common.cpp
        lib/skin/hc-skins.cpp
        lib/skin/ini-file.cpp
        lib/skin/lines.cpp
        lib/strutil/filevercmp.cpp
        lib/strutil/replace.cpp
        lib/strutil/strescape.cpp
        lib/strutil/strutil.cpp
        lib/strutil/strutil8bit.cpp
        lib/strutil/strutilascii.cpp
        lib/strutil/strutilutf8.cpp
        lib/strutil/strverscmp.cpp
        lib/strutil/xstrtol.cpp
        lib/tty/color-internal.cpp
#        lib/tty/color-ncurses.c
        lib/tty/color-slang.cpp
        lib/tty/color.cpp
        lib/tty/key.cpp
        lib/tty/keyxdef.cpp
        lib/tty/mouse.cpp
        lib/tty/tty-internal.cpp
#        lib/tty/tty-ncurses.c
        lib/tty/tty-slang.cpp
        lib/tty/tty.cpp
        lib/tty/win.cpp
        lib/tty/x11conn.cpp
        lib/vfs/direntry.cpp
        lib/vfs/gc.cpp
        lib/vfs/interface.cpp
        lib/vfs/netutil.cpp
        lib/vfs/parse_ls_vga.cpp
        lib/vfs/path.cpp
        lib/vfs/utilvfs.cpp
        lib/vfs/vfs.cpp
        lib/widget/background.cpp
        lib/widget/button.cpp
        lib/widget/buttonbar.cpp
        lib/widget/check.cpp
        lib/widget/dialog-switch.cpp
        lib/widget/dialog.cpp
        lib/widget/frame.cpp
        lib/widget/gauge.cpp
        lib/widget/group.cpp
        lib/widget/groupbox.cpp
        lib/widget/history.cpp
        lib/widget/hline.cpp
        lib/widget/input.cpp
        lib/widget/input_complete.cpp
        lib/widget/label.cpp
        lib/widget/listbox-window.cpp
        lib/widget/listbox.cpp
        lib/widget/menu.cpp
        lib/widget/mouse.cpp
        lib/widget/quick.cpp
        lib/widget/radio.cpp
        lib/widget/rect.cpp
        lib/widget/widget-common.cpp
        lib/widget/wtools.cpp
        lib/charsets.cpp
        lib/glibcompat.cpp
        lib/global.cpp
        lib/hook.cpp
        lib/keybind.cpp
        lib/lock.cpp
        lib/logging.cpp
        lib/serialize.cpp
        lib/shell.cpp
        lib/timefmt.cpp
        lib/timer.cpp
        lib/util.cpp
        lib/utilunix.cpp
#        maint/templates/template.cpp    TODO <System Headers> daes not found
#        src/consaver/cons.saver.cpp
        src/diffviewer/search.cpp
        src/diffviewer/ydiff.cpp
        src/editor/bookmark.cpp
        src/editor/choosesyntax.cpp
        src/editor/edit.cpp
        src/editor/editbuffer.cpp
        src/editor/editcmd.cpp
        src/editor/editcmd_dialogs.cpp
        src/editor/editdraw.cpp
        src/editor/editmenu.cpp
        src/editor/editoptions.cpp
        src/editor/editwidget.cpp
        src/editor/etags.cpp
        src/editor/format.cpp
#        src/editor/spell.cpp        TODO 'spell_language' not found
#        src/editor/spell_dialogs.cpp
        src/editor/syntax.cpp
        src/filemanager/achown.cpp
        src/filemanager/boxes.cpp
        src/filemanager/chattr.cpp
        src/filemanager/chmod.cpp
        src/filemanager/chown.cpp
        src/filemanager/cmd.cpp
        src/filemanager/command.cpp
        src/filemanager/dir.cpp
        src/filemanager/ext.cpp
        src/filemanager/file.cpp
        src/filemanager/filegui.cpp
        src/filemanager/filenot.cpp
        src/filemanager/fileopctx.cpp
        src/filemanager/find.cpp
        src/filemanager/hotlist.cpp
        src/filemanager/info.cpp
        src/filemanager/layout.cpp
        src/filemanager/listmode.cpp
        src/filemanager/midnight.cpp
        src/filemanager/mountlist.cpp
        src/filemanager/panel.cpp
        src/filemanager/panelize.cpp
        src/filemanager/tree.cpp
        src/filemanager/treestore.cpp
        src/subshell/common.cpp
        src/subshell/proxyfunc.cpp
        src/vfs/cpio/cpio.cpp
        src/vfs/extfs/extfs.cpp
        src/vfs/fish/fish.cpp
        src/vfs/ftpfs/ftpfs.cpp
        src/vfs/local/local.cpp
        src/vfs/sfs/sfs.cpp
        src/vfs/sftpfs/config_parser.cpp
        src/vfs/sftpfs/connection.cpp
        src/vfs/sftpfs/dir.cpp
        src/vfs/sftpfs/file.cpp
        src/vfs/sftpfs/init.cpp
        src/vfs/sftpfs/internal.cpp
        src/vfs/sftpfs/vfs_class.cpp
        src/vfs/sftpfs/vfs_subclass.cpp
#        src/vfs/smbfs/helpers/lib/charcnv.cpp
#        src/vfs/smbfs/helpers/lib/charset.cpp       TODO 'CODEPAGEDIR' not found
#        src/vfs/smbfs/helpers/lib/debug.cpp
#        src/vfs/smbfs/helpers/lib/interface.cpp
#        src/vfs/smbfs/helpers/lib/kanji.cpp
        src/vfs/smbfs/helpers/lib/md4.cpp
        src/vfs/smbfs/helpers/lib/netmask.cpp
        src/vfs/smbfs/helpers/lib/slprintf.cpp
        src/vfs/smbfs/helpers/lib/system.cpp
#        src/vfs/smbfs/helpers/lib/time.cpp
#        src/vfs/smbfs/helpers/lib/username.cpp
#        src/vfs/smbfs/helpers/lib/util.cpp
#        src/vfs/smbfs/helpers/lib/util_file.cpp
#        src/vfs/smbfs/helpers/lib/util_sock.cpp
#        src/vfs/smbfs/helpers/lib/util_str.cpp
#        src/vfs/smbfs/helpers/libsmb/clientgen.cpp
#        src/vfs/smbfs/helpers/libsmb/namequery.cpp   TODO 'LMHOSTSFILE' not found
#        src/vfs/smbfs/helpers/libsmb/nmblib.cpp
        src/vfs/smbfs/helpers/libsmb/nterr.cpp
#        src/vfs/smbfs/helpers/libsmb/pwd_cache.cpp
        src/vfs/smbfs/helpers/libsmb/smbdes.cpp
#        src/vfs/smbfs/helpers/libsmb/smbencrypt.cpp
#        src/vfs/smbfs/helpers/libsmb/smberr.cpp
#        src/vfs/smbfs/helpers/param/loadparm.cpp   TODO 'DRIVERFILE' not found
#        src/vfs/smbfs/helpers/param/params.cpp
#        src/vfs/smbfs/smbfs.cpp
        src/vfs/tar/tar.cpp
#        src/vfs/undelfs/undelfs.cpp    TODO some linker problems
        src/vfs/plugins_init.cpp
        src/viewer/actions_cmd.cpp
        src/viewer/ascii.cpp
        src/viewer/coord_cache.cpp
        src/viewer/datasource.cpp
        src/viewer/dialogs.cpp
        src/viewer/display.cpp
        src/viewer/growbuf.cpp
        src/viewer/hex.cpp
        src/viewer/lib.cpp
        src/viewer/mcviewer.cpp
        src/viewer/move.cpp
        src/viewer/nroff.cpp
        src/viewer/search.cpp
        src/args.cpp
        src/background.cpp
        src/clipboard.cpp
        src/cons.handler.cpp
        src/events_init.cpp
        src/execute.cpp
        src/file_history.cpp
        src/help.cpp
        src/keybind-defaults.cpp
        src/learn.cpp
        src/main.cpp
        src/selcodepage.cpp
        src/setup.cpp
        src/textconf.cpp
        src/usermenu.cpp
        src/util.cpp
#        tests/lib/mcconfig/config_string.c
#        tests/lib/mcconfig/user_configs_path.c
#        tests/lib/search/glob_prepare_replace_str.c
#        tests/lib/search/glob_translate_to_regex.c
#        tests/lib/search/hex_translate_to_regex.c
#        tests/lib/search/regex_process_escape_sequence.c
#        tests/lib/search/regex_replace_esc_seq.c
#        tests/lib/search/translate_replace_glob_to_regex.c
#        tests/lib/strutil/filevercmp.c
#        tests/lib/strutil/parse_integer.c
#        tests/lib/strutil/replace__str_replace_all.c
#        tests/lib/strutil/str_verscmp.c
#        tests/lib/vfs/canonicalize_pathname.c
#        tests/lib/vfs/current_dir.c
#        tests/lib/vfs/path_cmp.c
#        tests/lib/vfs/path_len.c
#        tests/lib/vfs/path_manipulations.c
#        tests/lib/vfs/path_recode.c
#        tests/lib/vfs/path_serialize.c
#        tests/lib/vfs/relative_cd.c
#        tests/lib/vfs/tempdir.c
#        tests/lib/vfs/vfs_adjust_stat.c
#        tests/lib/vfs/vfs_get_encoding.c
#        tests/lib/vfs/vfs_parse_ls_lga.c
#        tests/lib/vfs/vfs_path_from_str_flags.c
#        tests/lib/vfs/vfs_path_string_convert.c
#        tests/lib/vfs/vfs_prefix_to_class.c
#        tests/lib/vfs/vfs_s_get_path.c
#        tests/lib/vfs/vfs_setup_cwd.c
#        tests/lib/vfs/vfs_split.c
#        tests/lib/widget/complete_engine.c
#        tests/lib/widget/group_init_destroy.c
#        tests/lib/widget/hotkey_equal.c
#        tests/lib/widget/widget_find_by_id.c
#        tests/lib/library_independ.c
#        tests/lib/mc_build_filename.c
#        tests/lib/mc_realpath.c
#        tests/lib/name_quote.c
#        tests/lib/serialize.c
#        tests/lib/utilunix__my_system-common.c
#        tests/lib/utilunix__my_system-fork_child.c
#        tests/lib/utilunix__my_system-fork_child_shell.c
#        tests/lib/utilunix__my_system-fork_fail.c
#        tests/lib/x_basename.c
#        tests/src/editor/editcmd__edit_complete_word_cmd.c
#        tests/src/filemanager/do_cd_command.c
#        tests/src/filemanager/examine_cd.c
#        tests/src/filemanager/exec_get_export_variables_ext.c
#        tests/src/filemanager/filegui_is_wildcarded.c
#        tests/src/filemanager/get_random_hint.c
#        tests/src/vfs/extfs/helpers-list/mc_parse_ls_l.c
#        tests/src/execute__common.c
#        tests/src/execute__execute_external_editor_or_viewer.c
#        tests/src/execute__execute_get_external_cmd_opts_from_config.c
#        tests/src/execute__execute_with_vfs_arg.c
#        tests/mctest.h
        config.h
        version.h)

target_link_libraries(mc slang e2p ssh2 gmodule-2.0 pthread glib-2.0 util)
