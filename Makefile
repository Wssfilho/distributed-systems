CC=mpicc
CFLAGS=-Wall -O2
TARGET=eleicao

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

run: $(TARGET)
# 8 processos, primeiro numero depois de target é o ID do processo que cai, e o 
#segundo é o id do processo que inicia a eleição, o 0 indica que o processo que caiu não volta, se for 1
#ele volta a participar
	mpirun --oversubscribe -np 8 ./$(TARGET) 7 4 0

clean:
	rm -f $(TARGET)

.PHONY: all run clean
