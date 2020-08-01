#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <Windows.h>

void windowTaskbarSet(HWND hWnd, int visible)
{
    long exstyle = GetWindowLong(hWnd, GWL_EXSTYLE);
    //long ststyle = GetWindowLong(hWnd, GWL_STYLE);

    if (visible)
    {
        // show
        printf("\nShow");
        exstyle |= WS_EX_APPWINDOW;
        exstyle &= ~(WS_EX_TOOLWINDOW);
        //ststyle |= WS_VISIBLE;
    }
    else
    {
        // hide
        printf("\nHide");
        exstyle |= WS_EX_TOOLWINDOW;
        exstyle &= ~(WS_EX_APPWINDOW);
        //ststyle &= ~(WS_VISIBLE);
    }

    //ShowWindow(hWnd, SW_HIDE);

    SetWindowLong(hWnd, GWL_EXSTYLE, exstyle);
    //SetWindowLong(hWnd, GWL_STYLE, ststyle);

    //ShowWindow(hWnd, SW_SHOW);

    ShowWindow(hWnd, visible ? SW_SHOW : SW_HIDE);
}

#include "args.h"

struct myarguments
{
    enum mode {MODE_SHELL, MODE_CREATE, MODE_WINDOW, MODE_ERR} mode;
    char* window_name;
    char* shell_cmd;
    char* shell_verb;
    char* createproc;
    int hidden;

    HWND winHandle;
    HANDLE process;
};

static const struct arg_option options[] = {
    {
    's',
    "shell",
    2,
    "[verb] [command]",
    "Runs a command using ShellExecuteEx and waits for the process to spawn a window"
    },

    {
    'c',
    "create",
    1,
    "command",
    "Runs a command using CreateProcess and waits for the process to spawn a window"
    },

    {
    'w',
    "window",
    1,
    "name",
    "Finds a window with the given title"
    },

    {
        'h',
        "hidden",
        0,
        NULL,
        "Does not display the console"
    }
};

int parse_my_args(int index, char opt, char**args, void* state)
{
    struct myarguments* ARGS = (struct myarguments*)state;

    switch(opt)
    {
        case 's':
            ARGS->mode = MODE_SHELL;
            ARGS->shell_verb = args[0];
            ARGS->shell_cmd = args[1];
        break;

        case 'c':
            ARGS->mode = MODE_CREATE;
            ARGS->createproc = args[0];
        break;

        case 'w':
            ARGS->mode = MODE_WINDOW;
            ARGS->window_name = args[0];
        break;

        case 'h':
            ARGS->hidden = 1;
        break;

        default:
            return ARGS_UNKNOWN;
        break;
    }

    return ARGS_OK;
}

HWND procWaitWindow(HANDLE hProcess)
{
    HWND app_hWnd = NULL;
    DWORD pid = GetProcessId(hProcess);

    BOOL CALLBACK EnumWindowsProcMy(HWND test_hwnd, LPARAM lParam)
    {
        DWORD lpdwProcessId;
        GetWindowThreadProcessId(test_hwnd, &lpdwProcessId);

        if(lpdwProcessId == pid)
        {
            app_hWnd = test_hwnd;
            return FALSE;
        }

        return TRUE;
    }

    printf("\n waiting for window on PID %lu ... ", pid);
    do
    {
        if (WaitForSingleObject(hProcess, 500) != WAIT_TIMEOUT)
        {
            fprintf(stderr, "\nProcess terminated without creating a window!");
            CloseHandle(hProcess);
            return 0;
        }

        EnumWindows(EnumWindowsProcMy, 0);
    }
    while (app_hWnd == NULL);

    return app_hWnd;
}

#include <psapi.h>
#include <shlwapi.h>

HICON windowGetIcon(HWND win, HANDLE process)
{
    HICON icon;

    if ((icon = (HICON)SendMessage(win, WM_GETICON, ICON_SMALL, 0)) != NULL)
        printf("\nWM_GETICON ICON_SMAL");
    else if ((icon = (HICON)SendMessage(win, WM_GETICON, ICON_SMALL2, 0)) != NULL)
        printf("\nWM_GETICON ICON_SMALL2");
    else if ((icon = (HICON)SendMessage(win, WM_GETICON, ICON_BIG, 0)) != NULL)
        printf("\nWM_GETICON ICON_BIG");
    else if ((icon = (HICON)GetClassLongPtr(win, GCLP_HICONSM)) != NULL)
        printf("\nGCLP_HICONSM");
    else if ((icon = (HICON)GetClassLongPtr(win, GCLP_HICON)) != NULL)
        printf("\nGCLP_HICON");
    else if (process != NULL)
    {
        char exepath[MAX_PATH];
        if (GetModuleFileNameExA(process, NULL, exepath, MAX_PATH))
        {
            printf("\nAttempt load icon from file: %s", exepath);

            HMODULE hModule;
            if ((hModule = LoadLibrary(exepath)) != NULL)
            {
                WORD iconIndex;
                icon = ExtractAssociatedIconA(hModule, exepath, &iconIndex); //LoadIcon(hModule, IDI_WINLOGO);
            }
            else
                fprintf(stderr, "\nLoadLibrary error %lu", GetLastError());
        }
    }

    if (icon == 0)
    {
        icon = LoadIcon(0, IDI_APPLICATION);
        fprintf(stderr, "\nFailed to get icon - will use default!");
    }

    return icon;
}

void windowGetText(char* dest, int len, HWND win, HANDLE process)
{
    if (!GetWindowText(win, dest, len))
    {
        if (!SendMessage(win, WM_GETTEXT, len, (LPARAM)dest))
        {
            if (!GetModuleFileNameExA(process, NULL, dest, len))
            {
                fprintf(stderr, "\nFailed to get window name - using default: %lu", GetLastError());
                strcpy(dest, "AutoTray");
            }
            else
                strcpy(dest, PathFindFileNameA(dest));
        }
    }
}

HWND myFindWindow(char* name)
{
    HWND found = 0;

    BOOL CALLBACK EnumWindowsProcMy(HWND test_hwnd, LPARAM lParam)
    {
        if (!IsWindowVisible(test_hwnd))
            return TRUE;

        long style = GetWindowLong(test_hwnd, GWL_EXSTYLE);

        if ((style & WS_EX_TOOLWINDOW) && !(style & WS_EX_APPWINDOW))
            return TRUE;

        if (GetWindow(test_hwnd, GW_OWNER))
            return TRUE;

        char text[MAX_PATH];
        char classname[MAX_PATH];
        GetWindowText(test_hwnd, text, MAX_PATH);
        GetClassName( test_hwnd, classname, MAX_PATH);

        if (strcmp(name, text) != 0)
            return TRUE;

        if (strcmp(classname, "ApplicationFrameWindow") == 0)
            return TRUE;

        found = test_hwnd;

        return FALSE;
    }

    EnumWindows(EnumWindowsProcMy, 0);

    return found;
}

HWND createMsgOnlyWindow(WNDPROC lpfnWndProc)
{
    static const char* class_name = "AUTO_TRAY_CLASS";
    WNDCLASSEX wx = {};
    wx.cbSize = sizeof(WNDCLASSEX);
    wx.lpfnWndProc = lpfnWndProc;

    wx.lpszClassName = class_name;
    if ( !RegisterClassEx(&wx) )
    {
        fprintf(stderr, "\nError registering class");
        return 0;
    }

    return CreateWindowEx( 0, class_name, "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL );
}

int main(int argc, char* argv[])
{
    struct myarguments ARGS = {0};

    ARGS.mode = MODE_ERR;

    // parse the arguments
    if (args_parse(argc, argv, options, sizeof(options)/sizeof(options[0]), &ARGS, parse_my_args) != ARGS_OK)
    {
        SHOW_USAGE:
        args_print_usage(options, sizeof(options)/sizeof(options[0]));
        return -1;
    }
    else if (ARGS.mode == MODE_ERR)
        goto SHOW_USAGE;

    if (ARGS.hidden)
    {
        FreeConsole();
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }

    if (ARGS.mode == MODE_SHELL) // shell mode
    {
        printf("\nShellExecuteEx(%s): %s", ARGS.shell_verb, ARGS.shell_cmd);

        SHELLEXECUTEINFO shExInfo = {0};
        shExInfo.cbSize = sizeof(shExInfo);
        shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        shExInfo.hwnd = 0;
        shExInfo.lpVerb = ARGS.shell_verb;
        shExInfo.lpFile = ARGS.shell_cmd;
        shExInfo.lpParameters = NULL;
        shExInfo.lpDirectory = 0;
        shExInfo.nShow = SW_SHOW;
        shExInfo.hInstApp = 0;

        if (!ShellExecuteEx(&shExInfo))
        {
            fprintf(stderr, "\nShellExecuteEx failed: %lu", GetLastError());
            return -1;
        }

        if (shExInfo.hProcess <= 0)
        {
            fprintf(stderr, "\nShellExecuteEx did not spawn a process");
            return -1;
        }

        ARGS.process = shExInfo.hProcess;
        if ((ARGS.winHandle = procWaitWindow(ARGS.process)) <= 0)
            return -1;
    }
    else if (ARGS.mode == MODE_CREATE) // create mode
    {
        printf("\nCreateProcess(): %s", ARGS.createproc);

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        SECURITY_ATTRIBUTES sa1 = {0}, sa2 = {0};

        ZeroMemory( &si, sizeof(si) );
        si.cb = sizeof(si);
        ZeroMemory( &pi, sizeof(pi) );

        sa1.bInheritHandle = TRUE;
        sa2.bInheritHandle = TRUE;

        // Start the child process.
        if( !CreateProcess( NULL,   // No module name (use command line)
            ARGS.createproc,        // Command line
            &sa1,                   // Process handle IS inheritable
            &sa2,                   // Thread handle IS inheritable
            TRUE,                   // Set handle inheritance to TRUE
            0,                      // No creation flags
            NULL,                   // Use parent's environment block
            NULL,                   // Use parent's starting directory
            &si,                    // Pointer to STARTUPINFO structure
            &pi )                   // Pointer to PROCESS_INFORMATION structure
        )
        {
            fprintf(stderr, "\nCreateProcess failed (%lu).\n", GetLastError());
            return -1;
        }
        else
            printf("\nStated process with PID: %lu", pi.dwProcessId);

        ARGS.process = pi.hProcess;
        if ((ARGS.winHandle = procWaitWindow(ARGS.process)) <= 0)
            return -1;
    }
    else if (ARGS.mode == MODE_WINDOW) // window mode
    {
        printf("\nFindWindow(): '%s'", ARGS.window_name);

        if ((ARGS.winHandle = myFindWindow(ARGS.window_name)) <= 0)
        {
            fprintf(stderr, "\nWindow not found!");
            return -1;
        }

        DWORD lpdwProcessId;
        GetWindowThreadProcessId(ARGS.winHandle, &lpdwProcessId);

        printf(" (PID %lu)", lpdwProcessId);
        if ((ARGS.process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, TRUE, lpdwProcessId)) <= 0)
        {
            fprintf(stderr, "\n Could not query the process! %lu", GetLastError());
            return -1;
        }
    }

    // get the window name and icon
    char win_name[MAX_PATH] = "";
    windowGetText(win_name, MAX_PATH, ARGS.winHandle, ARGS.process);
    HICON icon = windowGetIcon(ARGS.winHandle, ARGS.process);
    printf("\nWill attach a tray icon to window '%s'", win_name);

    // the event handler for the tray icon
    LRESULT CALLBACK WindowProc(HWND cb_hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        static int visible;
        if (uMsg == WM_APP && lParam == WM_LBUTTONUP)
        {
            windowTaskbarSet(ARGS.winHandle, visible);
            visible = !visible;
        }

        return DefWindowProc(cb_hwnd, uMsg, wParam, lParam);
    }

    // create the message window
    HWND msg_window;

    if ((msg_window = createMsgOnlyWindow(WindowProc)) <= 0)
        return -1;

    // add a tray icon
    NOTIFYICONDATA nid = {0};
    nid.hWnd             = msg_window;
    nid.uID              = 0;
    nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP;
    nid.hIcon            = icon;
    strcpy(nid.szTip, win_name);

    if (!Shell_NotifyIcon( NIM_ADD, &nid ))
    {
        fprintf(stderr, "\nFailed to create tray icon");
        return -1;
    }

    // hide the terminal
    FreeConsole();
    HWND console_hWnd = GetConsoleWindow();
    ShowWindow( console_hWnd, SW_MINIMIZE );  //won't hide the window without SW_MINIMIZE
    ShowWindow( console_hWnd, SW_HIDE );

    // Main process loop
    MSG msg;
    while(1)
    {
        if (WaitForSingleObject(ARGS.process, 100) != WAIT_TIMEOUT)
        {
            printf("\nProcess exited!");
            break;
        }

        if (PeekMessage(&msg, msg_window, 0, 0, PM_NOREMOVE) != 0)
        {
            if (GetMessage(&msg, msg_window, 0, 0) != 0)
                DispatchMessage(&msg);
            else
                break;
        }
    }

    // Close process and thread handles.
    Shell_NotifyIcon( NIM_DELETE, &nid);
    CloseHandle( ARGS.process );

    return 0;
}
