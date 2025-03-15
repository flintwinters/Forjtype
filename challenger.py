#!/usr/bin/python3.11
from sys import argv
from toml import loads
from os import system
from os import path
import re

T = {} 
with open("challenges.toml", "r") as f:
    T = loads(f.read())

def fail(name, s):
    print(f"\033[31;1m{name} "+"─"*30+"\033[0m\n"+s)

def replaceall(s, r):
    for k, v in r:
        s = s.replace(k, v)
    return re.sub('\s+', ' ', s)

def runforj(v):
    with open("challenge", "w") as f:
        f.write(v["challenge"]+" ")
    if system("./fj > challengeresult"):
    # if system("make val --no-print-directory > challengeresult"):
        with open("challengeresult", "r") as f:
            s = f.read()
            return "execution returned error code"+s
    ansi_escape = re.compile(r'\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])')
    s = ""
    with open("challengeresult", "r") as f:
        s = f.read()
    st = ansi_escape.sub('', s).strip()
    if 'result' not in v:
        return "No result found"
    r = v['result'].strip()
    if r != st:
        return "expected:\n"+v["result"]+"\nactual:\n"+s

def main():
    ret = 0
    failed = False
    system("rm fj; make --no-print-directory fj")
    for k, v in T.items():
        s = runforj(v)
        if s:
            fail(k, s)
            system('make gdb')
            failed = True

    if not failed:
        system("make --no-print-directory val 1> /dev/null")
        print("\033[92;1mfj all pass "+"\033[0m")
    if path.exists("challengeresult"):
        system("rm challengeresult")
    return ret

exit(main())
