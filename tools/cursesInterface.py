import curses
import subprocess
from curses import wrapper
from curses.textpad import rectangle

def button(win, str, y, x):
    rectangle(win, y, x, y+2, x+len(str)+1)
    win.addstr(y+1, x+1, str)

def get_module_load(module_name):
    """Checks if module is loaded"""
    lsmod_proc = subprocess.Popen(['lsmod'], stdout=subprocess.PIPE)
    grep_proc = subprocess.Popen(['grep', module_name], stdin=lsmod_proc.stdout, stdout=subprocess.PIPE)
    grep_proc.communicate()  # Block until finished
    return grep_proc.returncode == 0

def main(stdscr):
    #default values
    def_freq = freq = 28
    def_state = state = "off"

    while 1:
        y_max, x_max = stdscr.getmaxyx()
        stopy = y_max-2-3
        stopx = x_max-1-2-len("stop")
        loady = 1
        loadx = int(round(x_max/2))
        stdscr.nodelay(1)
        stdscr.timeout(400)
        curses.curs_set(False)
        curses.mousemask(1)
        # Clear screen
        stdscr.clear()

        rectangle(stdscr, 0, 0, y_max-2, x_max-1)
        stdscr.refresh()
        loaded = get_module_load("recode")
        module_load = "loaded" if loaded else "unloaded"
        stdscr.addstr(2, 2, "module: {}" .format(module_load))
        button(stdscr, "unload" if loaded else "load", loady, loadx)
        stdscr.addstr(4, 2, "frequency: {}" .format(freq))
        stdscr.addstr(4, loadx, "-  +")
        stdscr.addstr(6, 2, "state: {}" .format(state))
        new_state = "system" if state=="off" else "off"
        stdscr.addstr(6, loadx, new_state)

        button(stdscr, "stop", stopy, stopx)
        stdscr.refresh()

        key = stdscr.getch()
        if (key == curses.KEY_MOUSE):
            _, x, y, _, _ = curses.getmouse()
            if (y in range(loady+1, loady+3) and x in range(loadx, loadx+(7 if loaded else 5))):
                if (loaded):
                    subprocess.Popen(['./recode.py', 'module', '-u'], stdout=subprocess.PIPE)
                    freq = def_freq
                    state = def_state
                else:
                    subprocess.Popen(['./recode.py', 'module', '-c', '-ld'], stdout=subprocess.PIPE)
            if (y == 4 and x == loadx):
                freq = freq-1
                subprocess.Popen(['./recode.py', 'config', '-f', str(freq)], stdout=subprocess.PIPE)
            if (y == 4 and x == loadx+3):
                freq = freq+1
                subprocess.Popen(['./recode.py', 'config', '-f', str(freq)], stdout=subprocess.PIPE)
            if (y == 6 and x in range(loadx, loadx+len(new_state))):
                subprocess.Popen(['./recode.py', 'config', '-s', new_state], stdout=subprocess.PIPE)
                state = new_state
            if (y in range(stopy+1, stopy+3) and x in range(stopx, stopx+len("stop"))):
                break

wrapper(main)
