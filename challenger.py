#!/usr/bin/python3.11
from pyperclip import copy
from sys import argv
from toml import loads
from os import system
from os import path
import re
from pyautogui import hotkey

T = {} 
with open("challenges.toml", "r") as f:
    T = loads(f.read())

def fail(name, s):
    print(f"\033[31;1m{name} "+"─"*20+"\033[0m\n"+s)

def replaceall(s, r):
    for k, v in r:
        s = s.replace(k, v)
    return re.sub('\s+', ' ', s)

def runforj(v, val=False):
    with open("challenge", "w") as f:
        f.write(v["challenge"])
    sy = 0
    if val:
        sy = system("make val --no-print-directory > challengeresult")
    else:
        sy = system("./fj > challengeresult")
    if sy:
        with open("challengeresult", "r") as f:
            s = f.read()
            return "execution returned error code"+s
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    
    s = ""
    with open("challengeresult", "r") as f:
        s = f.read()
    st = ansi_escape.sub('', s).strip()
    st = st.replace("┃", " ");
    if 'result' not in v:
        return "No result found"
    r = v['result'].strip()
    if r != st:
        # copy(st)
        return "expected:\n"+v["result"]+"\nactual:\n"+s

def main():
    ret = 0
    failed = False
    if path.exists("fj"):
        system("rm fj")
    system("make --no-print-directory fj")
    s = ""
    if len(argv) > 1:
        s = runforj(T[argv[1]], val=True)
        if s:
            fail(argv[1], s)
            failed = True
    else:
        for k, v in T.items():
            s = runforj(v)
            if s:
                fail(k, s)
                # input()
                failed = True
    if failed:
        system('make gdb')
    else:
        system("make --no-print-directory val 1> /dev/null")
        if len(argv) > 1 and argv[1]:
            print(f"\033[92;1mpassed {argv[1]}"+"\033[0m")
        else:
            print("\033[92;1mall pass "+"\033[0m")
    if path.exists("challengeresult"):
        system("rm challengeresult")
    return ret

exit(main())
