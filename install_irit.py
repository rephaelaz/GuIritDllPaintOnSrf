import os
import sys
import time
from shutil import copyfile

irit_vs12_lib = 'C:\\irit\\irit\\windowsVC2012\\lib'
irit_vs12_ntbin = 'C:\\irit\\irit\\windowsVC2012\\ntbin'
irit_vs19_lib = 'C:\\irit\\irit\\windowsVC2019\\lib'
irit_vs19_ntbin = 'C:\\irit\\irit\\windowsVC2019\\ntbin'
irit_lib = 'C:\\irit\\irit\\lib'
irit_ntbin = 'C:\\irit\\irit\\ntbin'


def copy_dir(old_path, new_path):
    files_old = [os.path.join(old_path, lib) for lib in os.listdir(old_path)]
    files_new = [os.path.join(new_path, lib) for lib in os.listdir(old_path)]
    for old, new in zip(files_old, files_new):
        print(f'Copying {old} to {new}')
        copyfile(old, new)  # Delete already existing file at new path if there is 
    print('')


if __name__ == "__main__":
    irit_vs_lib = irit_vs19_lib
    irit_vs_ntbin = irit_vs19_ntbin
    if len(sys.argv) == 2:
        if sys.argv[1] == '2012':
            irit_vs_lib = irit_vs12_lib
            irit_vs_ntbin = irit_vs12_ntbin
    copy_dir(irit_vs_lib, irit_lib)
    if not os.path.exists(irit_ntbin):
        os.makedirs(irit_ntbin)
    copy_dir(irit_vs_ntbin, irit_ntbin)
    print('DONE')
    time.sleep(2)
