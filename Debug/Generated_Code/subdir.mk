################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Generated_Code/Cpu.c \
../Generated_Code/KSDK1.c \
../Generated_Code/adConv1.c \
../Generated_Code/adConv2.c \
../Generated_Code/clockMan1.c \
../Generated_Code/eNet1.c \
../Generated_Code/fsl_sdhc1.c \
../Generated_Code/gpio1.c \
../Generated_Code/hardware_init.c \
../Generated_Code/memoryCard1.c \
../Generated_Code/osa1.c \
../Generated_Code/pin_mux.c \
../Generated_Code/pitTimer1.c \
../Generated_Code/pitTimer2.c \
../Generated_Code/pitTimer3.c \
../Generated_Code/pitTimer4.c 

OBJS += \
./Generated_Code/Cpu.o \
./Generated_Code/KSDK1.o \
./Generated_Code/adConv1.o \
./Generated_Code/adConv2.o \
./Generated_Code/clockMan1.o \
./Generated_Code/eNet1.o \
./Generated_Code/fsl_sdhc1.o \
./Generated_Code/gpio1.o \
./Generated_Code/hardware_init.o \
./Generated_Code/memoryCard1.o \
./Generated_Code/osa1.o \
./Generated_Code/pin_mux.o \
./Generated_Code/pitTimer1.o \
./Generated_Code/pitTimer2.o \
./Generated_Code/pitTimer3.o \
./Generated_Code/pitTimer4.o 

C_DEPS += \
./Generated_Code/Cpu.d \
./Generated_Code/KSDK1.d \
./Generated_Code/adConv1.d \
./Generated_Code/adConv2.d \
./Generated_Code/clockMan1.d \
./Generated_Code/eNet1.d \
./Generated_Code/fsl_sdhc1.d \
./Generated_Code/gpio1.d \
./Generated_Code/hardware_init.d \
./Generated_Code/memoryCard1.d \
./Generated_Code/osa1.d \
./Generated_Code/pin_mux.d \
./Generated_Code/pitTimer1.d \
./Generated_Code/pitTimer2.d \
./Generated_Code/pitTimer3.d \
./Generated_Code/pitTimer4.d 


# Each subdirectory must supply rules for building sources it contributes
Generated_Code/%.o: ../Generated_Code/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g3 -D"CPU_MK64FX512VLL12" -D"CD_USING_GPIO" -D"SD_DISK_ENABLE=1" -D"FSL_OSA_BM_TIMER_CONFIG=0" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/hal/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Middleware/fatfs" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Middleware/fatfs/fsl_sd_disk" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/hal/src/sim/MK64F12" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/system/src/clock/MK64F12" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/system/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/osa/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/CMSIS/Include" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/devices" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/devices/MK64F12/include" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/utilities/src" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/utilities/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/devices/MK64F12/startup" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Generated_Code/SDK/platform/devices/MK64F12/startup" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Sources" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Generated_Code" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/drivers/inc" -I"..\lwip\src\include" -I"..\lwip\src\include\lwip" -I"..\lwip\src\include\ipv4" -I"..\lwip\port" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Static_Code/IO_Map" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Static_Code/Peripherals" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/composite/inc" -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


