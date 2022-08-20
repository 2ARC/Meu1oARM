################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../lwip/src/core/ipv4/autoip.c \
../lwip/src/core/ipv4/icmp.c \
../lwip/src/core/ipv4/igmp.c \
../lwip/src/core/ipv4/inet.c \
../lwip/src/core/ipv4/inet_chksum.c \
../lwip/src/core/ipv4/ip.c \
../lwip/src/core/ipv4/ip_addr.c \
../lwip/src/core/ipv4/ip_frag.c 

OBJS += \
./lwip/src/core/ipv4/autoip.o \
./lwip/src/core/ipv4/icmp.o \
./lwip/src/core/ipv4/igmp.o \
./lwip/src/core/ipv4/inet.o \
./lwip/src/core/ipv4/inet_chksum.o \
./lwip/src/core/ipv4/ip.o \
./lwip/src/core/ipv4/ip_addr.o \
./lwip/src/core/ipv4/ip_frag.o 

C_DEPS += \
./lwip/src/core/ipv4/autoip.d \
./lwip/src/core/ipv4/icmp.d \
./lwip/src/core/ipv4/igmp.d \
./lwip/src/core/ipv4/inet.d \
./lwip/src/core/ipv4/inet_chksum.d \
./lwip/src/core/ipv4/ip.d \
./lwip/src/core/ipv4/ip_addr.d \
./lwip/src/core/ipv4/ip_frag.d 


# Each subdirectory must supply rules for building sources it contributes
lwip/src/core/ipv4/%.o: ../lwip/src/core/ipv4/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross ARM C Compiler'
	arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -O0 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -Wall  -g3 -D"CPU_MK64FX512VLL12" -D"CD_USING_GPIO" -D"SD_DISK_ENABLE=1" -D"FSL_OSA_BM_TIMER_CONFIG=0" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/hal/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Middleware/fatfs" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Middleware/fatfs/fsl_sd_disk" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/hal/src/sim/MK64F12" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/system/src/clock/MK64F12" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/system/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/osa/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/CMSIS/Include" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/devices" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/devices/MK64F12/include" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/utilities/src" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/utilities/inc" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/devices/MK64F12/startup" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Generated_Code/SDK/platform/devices/MK64F12/startup" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Sources" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Generated_Code" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/drivers/inc" -I"..\lwip\src\include" -I"..\lwip\src\include\lwip" -I"..\lwip\src\include\ipv4" -I"..\lwip\port" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Static_Code/IO_Map" -I"C:/Users/Marco/workspace.kds/Meu1oARM/Static_Code/Peripherals" -I"C:/Users/Marco/workspace.kds/Meu1oARM/SDK/platform/composite/inc" -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


