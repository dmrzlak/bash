CC = gcc
CLFAGS = -g -Wall

CLEAN = clean
TARGET = main

GEN = *.txt
BUFFER = *~

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

clean:
	@- $(RM) $(TARGET)
	@- $(RM) $(GEN)

package:
	@- $(RM) $(TARGET)
	@- $(RM) $(GEN)
	@- $(RM) $(BUFFER)
