CC=mpicc
CFLAGS=-Wall -O2
TARGET=eleicao

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

run: $(TARGET)
# 8 processes. The first number after the target is the ID of the process that goes down,
# and the second is the ID of the process that detects the failure and starts the election.
# The trailing 0 indicates the failed process will NOT return; if it is 1, it returns.
	mpirun --oversubscribe -np 8 ./$(TARGET) 7 4 0

clean:
	rm -f $(TARGET)

.PHONY: all run clean
