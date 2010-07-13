################################################################################
# Automatically-generated file. Do not edit!
################################################################################

#-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
endif

#-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: scfg

# Tool invocations

# scfg.o: ../scfg.cpp
# 	@echo 'Building file: $<'
# 	@echo 'Invoking: GCC C++ Compiler'
# 	g++ -O3 -g3 -Wall -c -fmessage-length=0 -I../../openfst-1.1/src/include/ -L../../openfst-1.1/src/lib/ -lfst  -lpthread -ldl -lm -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
# 	@echo 'Finished building: $<'
# 	@echo ' '

scfg: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C++ Linker'
	/bin/sh ../../../../libtool --tag=CXX   --mode=link g++  -g -O2 -lz -L/export/ws10smt/software/lib -R/export/ws10smt/software/lib -L/export/ws10smt/software/srilm-1.5.10/lib/i686 -o scfg $(OBJS) -L/export/ws10smt/software/lib -lgtest -pthread ../../../../decoder/libcdec.a -lboost_program_options -loolm -ldstruct -lmisc	
	@echo 'Finished building target: $@'
	@echo ' '
#g++ -I/home/tnguyen/ws10smt/gi/scfg/cdec/ -I/export/ws10smt/software/srilm-1.5.10/include/ -L/home/tnguyen/ws10smt/decoder  -lpthread -ldl -lm  $(OBJS) $(USER_OBJS) $(LIBS) -o"scfg"
# Other Targets
clean:
	-$(RM) $(OBJS)$(C++_DEPS)$(EXECUTABLES)$(CC_DEPS)$(C_DEPS)$(CPP_DEPS)$(CXX_DEPS)$(C_UPPER_DEPS) scfg
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
