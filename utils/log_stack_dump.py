import sys
import subprocess
from parse import *

filename = sys.argv[1]
elf_file = sys.argv[2]

with open(filename, 'r') as fd:
    data = fd.readlines()

def addr2line(elf, pc):
    try:
        cmd = ['addr2line', '-e', elf, '-f', pc]
        with subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as process:
            op, err = process.communicate()
    finally:
        process.terminate()
    return op.decode('ascii').strip()
 
def is_stack_trace(line):
    if 'PC[' in line:
        return True
    return False

def get_pc_from_trace(line):
    pre, post = line.split(' = ')
    result = parse('[{}]', pre.split('PC')[1])
    return result[0], post

for index, value in enumerate(data):
    if is_stack_trace(value):
        offset, pc = get_pc_from_trace(value.strip())
        op = addr2line(elf_file, pc)
        print(f'PC {offset} on log line #{index} {pc} {op}')
