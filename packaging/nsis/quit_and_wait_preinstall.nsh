; If kw_legion is already running, ask it to quit over its
; SingleInstanceGuard local-socket pipe (see
; kwlegion_ui/cpp/singleinstanceguard.cpp) before overwriting its files.
;
; This lives in its own .nsh file, !include'd from CPACK_NSIS_EXTRA_
; PREINSTALL_COMMANDS (see CMakeLists.txt), rather than being embedded
; directly as that variable's value: CPack's NSIS generator mangles
; embedded double quotes in that variable (confirmed - a two-argument
; NSIS command like Rename "X" "Y" gets its outer quotes silently replaced
; with semicolons somewhere in CPack's own generator), so anything with
; more than trivial quoting needs to live in a real file instead.

; Send Request::Quit (value 1) to the app's local-socket pipe. If
; nothing is listening, CreateFileW just fails and we fall through -
; the check below will find the exe already unlocked.
;
; Note: a pre-0.3.0 install shares this same pipe name for its own
; single-instance handling, but doesn't understand this request - the
; connection attempt alone still wakes its "another instance launched"
; handler, which raises its window. That's an unavoidable side effect of
; upgrading from a version older than this quit protocol, not something
; this script can suppress.
System::Call 'kernel32::CreateFileW(w "\\.\pipe\kw_legion", i 0x40000000, i 0, i 0, i 3, i 0, i 0) i .r0'
IntCmp $0 -1 legion_pipe_absent legion_pipe_connected legion_pipe_connected
legion_pipe_connected:
  System::Call '*(&i1 1) i .r1'
  System::Call 'kernel32::WriteFile(i r0, i r1, i 1, *i .r2, i 0) i .r3'
  System::Free $1
  System::Call 'kernel32::CloseHandle(i r0)'
legion_pipe_absent:

; If the pipe-based quit request above didn't actually get the app to
; exit (e.g. an old version that doesn't understand it, or the user has
; unsaved state it's waiting on), don't loop on a Retry modal - just tell
; the user why installation can't continue and abort. They can re-run
; this installer once it's actually closed. The check itself is whether
; the exe can be opened for exclusive write access (not renamed to
; itself, which turned out to report success regardless of whether the
; process is running at all and so never reflected real lock state).
legion_wait_check:
  ; CreateFileW(OPEN_EXISTING) returns INVALID_HANDLE_VALUE (-1) both when
  ; the file is locked by a running process AND when it simply doesn't
  ; exist yet (e.g. no prior install at this path, or a fresh install) -
  ; confirmed the two are indistinguishable from the handle alone. Without
  ; this existence check first, a fresh/first-time install would always
  ; get flagged as "still running" even with nothing running at all.
  IfFileExists "$INSTDIR\kw_legion.exe" legion_check_lock legion_wait_done

legion_check_lock:
  System::Call 'kernel32::CreateFileW(w "$INSTDIR\kw_legion.exe", i 0x40000000, i 0, i 0, i 3, i 0, i 0) i .r3'
  IntCmp $3 -1 legion_still_running
  System::Call 'kernel32::CloseHandle(i r3)'
  Goto legion_wait_done

legion_still_running:
  ; Reclaim the foreground in case the old process's own activation (see
  ; above) left this installer's window - and the message below - behind
  ; it rather than on top.
  System::Call 'user32::SetForegroundWindow(i $HWNDPARENT)'
  ; Deliberately says "quit", not "close": this app hides to the tray on
  ; a plain window close by default (Main.qml's onClosing, when
  ; Settings.closeToTray is on) rather than exiting, so telling someone
  ; to "close" it does nothing. The only things that actually end the
  ; process are the tray icon's own Quit menu item or killing it
  ; directly.
  MessageBox MB_OK|MB_ICONEXCLAMATION \
    "KW Legion is still running and needs to quit before installing."
  Quit

legion_wait_done:
