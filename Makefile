CC=mpicc
CFLAGS=-Wall -O2
TARGET=bully_algorithm

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

run: $(TARGET)
	mpirun --oversubscribe -np 10 ./$(TARGET) 1 6 0

clean:
	rm -f $(TARGET)

.PHONY: all run clean
