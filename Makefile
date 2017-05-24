all: pov_sample.hex

pov_sample.hex: pov_sample.elf
	avr-objcopy  -j .text -j .data -O ihex $^ $@
	avr-size pov_sample.elf

pov_sample.elf: pov_sample.c
	avr-g++ -mmcu=atmega324a -Os -Wall -o $@ $^

clean:
	rm -rf pov_sample.elf pov_sample.hex
