CC=mpicc
CFLAGS=-Wall -O2
TARGET=eleicao

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c

run: $(TARGET)
<<<<<<< HEAD
	mpirun --oversubscribe -np 7 ./$(TARGET) 7 4
=======
# 10 processos, primeiro numero depois de target é o ID do processo que cai, e o 
#segundo é o id do processo que inicia a eleição
	mpirun --oversubscribe -np 8 ./$(TARGET) 7 4 0
>>>>>>> refatoração

clean:
	rm -f $(TARGET)

.PHONY: all run clean
