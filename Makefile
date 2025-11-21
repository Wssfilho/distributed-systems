CC=mpicc
CFLAGS=-Wall -O2
TARGET=bully_algorithm

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

run: $(TARGET)
	mpirun --oversubscribe -np 7 ./$(TARGET) 7 4

clean:
	rm -f $(TARGET)

.PHONY: all run clean
