.PHONY: all
all: compile upload

.PHONY: compile
compile:
	arduino-cli compile --fqbn esp8266:esp8266:d1_mini

.PHONY: upload
upload:
	arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:d1_mini

.PHONY: clean
clean:
	rm -rf build
