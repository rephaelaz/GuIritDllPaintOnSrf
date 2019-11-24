import os
from shutil import copyfile, copytree

guirit_ntbin = 'C:\irit\extra\GuIrit\Src\WindowsVC2012\\ntbin'
irit_ntbin = 'C:\irit\irit\\ntbin'
irit_ext = 'C:\irit\irit\\ntbin\GuIritData\Extensions'

if __name__ == "__main__":
    guirit_exe = os.path.join(guirit_ntbin, 'GuIritD.exe')
    irit_exe = os.path.join(irit_ntbin, 'GuIritD.exe')
    copyfile(guirit_exe, irit_exe)
    print(f'Copying {guirit_exe} to {irit_exe}')

    config = 'C:\irit\extra\GuIrit\Src\GuIrit\GuIrit.cfg.Win'
    config_dest = os.path.join(irit_ntbin, 'GuIrit.cfg')
    print(f'Copying {config} to {config_dest}')
    copyfile(config, config_dest)

    guirit_dll = os.path.join(guirit_ntbin, 'GuIritDllExtensionsD.dll')
    irit_dll = os.path.join(irit_ntbin, 'GuIritDllExtensionsD.dll')
    copyfile(guirit_dll, irit_dll)
    print(f'Copying {guirit_dll} to {irit_dll}\n')

    guirit_data = 'C:\irit\extra\GuIrit\Src\RunTime\GuIritData'
    irit_data = 'C:\irit\irit\\ntbin\GuIritData'
    if not os.path.exists(irit_data):
        print(f'Copying {guirit_data} to {irit_data}\n')
        copytree(guirit_data, irit_data)

    exts = [dll for dll in os.listdir(guirit_ntbin) if dll.endswith('dll') and dll != 'GuIritDllExtensionsD.dll']
    guirit_exts = [os.path.join(guirit_ntbin, ext) for ext in exts]
    irit_exts = [os.path.join(irit_ext, ext) for ext in exts]
    for old, new in zip(guirit_exts, irit_exts):
        print(f'Copying {old} to {new}')
        copyfile(old, new)
    
    print('\nDONE')
