cd C:/Users/marco/workspace.kds/Meu1oARM
start cmd /k GDB.bat
sleep 1
cd C:/Freescale/KDS_v3/Toolchain/bin
arm-none-eabi-gdb.exe -x C:/Users/marco/workspace.kds/Meu1oARM/Meu1oARM.txt
TASKKILL /F /IM cmd.exe /T
