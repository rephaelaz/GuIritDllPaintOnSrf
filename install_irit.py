import os
from shutil import copyfile

irit_vs12_lib = 'C:\irit\irit\windowsVC2012\lib'
irit_vs12_ntbin = 'C:\irit\irit\windowsVC2012\\ntbin'
irit_lib = 'C:\irit\irit\lib'
irit_ntbin = 'C:\irit\irit\\ntbin'

def copy_dir(old_path, new_path):
    files_old = [os.path.join(old_path, lib) for lib in os.listdir(old_path)]
    files_new = [os.path.join(new_path, lib) for lib in os.listdir(old_path)]
    for old, new in zip(files_old, files_new):
        print(f'Copying {old} to {new}')
        copyfile(old, new)  # Delete already existing file at new path if there is 
    print('')

if __name__ == "__main__":
    copy_dir(irit_vs12_lib, irit_lib)
    copy_dir(irit_vs12_ntbin, irit_ntbin)
    print('DONE')
