; AutoHotkey v2 script for productivity
; Demonstrates classes, methods, functions, hotkeys

#HotIf WinActive("ahk_class Notepad")

; Toggle word wrap in Notepad
^w::Send("!oW")

; Quick save shortcut
^s::Send("^s")

#HotIf

; Global hotkey for calculator
#c::Run("calc.exe")

; Utility function to center a window
; on the primary monitor
CenterWindow(winTitle) {
    WinGetPos(&x, &y, &w, &h, winTitle)
    MonitorGet(1, &ml, &mt, &mr, &mb)
    newX := (mr - ml - w) / 2
    newY := (mb - mt - h) / 2
    WinMove(newX, newY, , , winTitle)
}

; Show a tooltip notification
ShowNotify(msg, duration := 2000) {
    ToolTip(msg)
    SetTimer(() => ToolTip(), -duration)
}

; Window manager class
class WindowManager {
    ; Track managed windows
    static instances := Map()

    __New(title) {
        this.title := title
        this.isMinimized := false
    }

    ; Minimize managed window
    Minimize() {
        WinMinimize(this.title)
        this.isMinimized := true
    }

    ; Restore managed window
    Restore() {
        WinRestore(this.title)
        this.isMinimized := false
    }

    ; Toggle minimize state
    Toggle() {
        if this.isMinimized
            this.Restore()
        else
            this.Minimize()
    }

    ; Static factory method
    static GetOrCreate(title) {
        if !WindowManager.instances.Has(title)
            WindowManager.instances[title] := WindowManager(title)
        return WindowManager.instances[title]
    }
}

class KeyLogger extends WindowManager {
    __New(title, logFile) {
        super.__New(title)
        this.logFile := logFile
    }

    ; Log a keystroke
    LogKey(key) {
        FileAppend(key, this.logFile)
    }
}
