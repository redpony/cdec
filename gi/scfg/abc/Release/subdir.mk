
################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../../utils/Util.cc \
../agrammar.cc \
../scfg.cpp


OBJS += \
./Util.o \
./agrammar.o \
./scfg.o 


CPP_DEPS += \
./Util.d \
./agrammar.d \
./scfg.d

# Each subdirectory must supply rules for building sources it contributes
# %.o: ../%.cpp
# 	@echo 'Building file: $<'
# 	@echo 'Invoking: GCC C++ Compiler'
# 	g++ -g -p -g3 -Wall -c -fmessage-length=0 -I../../openfst-1.1/src/include/ -L../../openfst-1.1/src/lib/ -lfst  -lpthread -ldl -lm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
# 	
# 	@echo ' '

%.o: ../../utils/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -g -p -g3 -Wall -c -fmessage-length=0  -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../../utils/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -g -p -g3 -Wall -c -fmessage-length=0  -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O3 -g3 -Wall -c -fmessage-length=0 -I../../utils/ -I/home/tnguyen/ws10smt/decoder -I/export/ws10smt/software/include -I/export/ws10smt/software/srilm-1.5.10/include -lpthread -ldl -lm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O3 -g3 -Wall -c -fmessage-length=0  -I../../utils/ -I/home/tnguyen/ws10smt/decoder -I/export/ws10smt/software/include -I/export/ws10smt/software/srilm-1.5.10/include -lpthread -ldl -lm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

