# MSP430 Makefile
# #####################################
#
# Part of the uCtools project
# uctools.github.com
#
#######################################
# user configuration:
#######################################
# TARGET: name of the output file
TARGET = openstep-firmware
# MCU: part number to build for
MCU = msp430g2553
# SOURCES: list of input source sources
SOURCES = main.c
# INCLUDES: list of includes, by default, use Includes directory
INCLUDES =
# OUTDIR: directory to use for output
OUTDIR = build
# define flags
CFLAGS = -mmcu=$(MCU) -g -Os -Wall -Wunused $(INCLUDES) -std=c99
ASFLAGS = -mmcu=$(MCU) -x assembler-with-cpp -Wa,-gstabs
LDFLAGS = -mmcu=$(MCU) -Wl,-Map=$(OUTDIR)/$(TARGET).map
#######################################
# end of user configuration
#######################################
#
#######################################
# binaries
#######################################
CC      	= msp430-gcc
LD      	= msp430-ld
AR      	= msp430-ar
AS      	= msp430-gcc
GASP    	= msp430-gasp
NM      	= msp430-nm
OBJCOPY 	= msp430-objcopy
MAKETXT 	= srec_cat
UNIX2DOS	= unix2dos
RM      	= rm -f
MKDIR		= mkdir -p
#######################################

# file that includes all dependencies
DEPEND = $(SOURCES:.c=.d)

# list of object files, placed in the build directory regardless of source path
OBJECTS = $(addprefix $(OUTDIR)/,$(notdir $(SOURCES:.c=.o)))

# default: build hex file and TI TXT file
all: $(OUTDIR)/$(TARGET).hex $(OUTDIR)/$(TARGET).txt

# TI TXT file
$(OUTDIR)/%.txt: $(OUTDIR)/%.hex
	$(MAKETXT) -O $@ -TITXT $< -I
	iconv -f utf-8 -t cp850 -o iconv.tmp $(OUTDIR)/$(TARGET).txt
	mv iconv.tmp $(OUTDIR)/$(TARGET).txt

# intel hex file
$(OUTDIR)/%.hex: $(OUTDIR)/%.elf
	$(OBJCOPY) -O ihex $< $@

# elf file
$(OUTDIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) $(LIBS) -o $@

$(OUTDIR)/%.o: %.c | $(OUTDIR)
	$(CC) -c $(CFLAGS) -o $@ $<

# assembly listing
%.lst: %.c
	$(CC) -c $(ASFLAGS) -Wa,-anlhd $< > $@

# create the output directory
$(OUTDIR):
	$(MKDIR) $(OUTDIR)

# remove build artifacts and executables
clean:
	-$(RM) $(OUTDIR)/*

# build in docker buildbox
vm:
	docker run -i -v $(shell realpath .):/root/build -w /root/build msp-buildbox make

.PHONY: all clean vm
