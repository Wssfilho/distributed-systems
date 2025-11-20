# Algoritmo do Valentão (Bully Algorithm) com MPI

Implementação do algoritmo de eleição de líder em sistemas distribuídos usando MPI (Message Passing Interface).

## Descrição do Algoritmo

O Algoritmo do Valentão é usado para eleição de coordenador em sistemas distribuídos. O processo funciona da seguinte forma:

1. **Detecção de Falha**: Quando um processo detecta que o coordenador falhou, inicia uma eleição
2. **Mensagem ELECTION**: Envia mensagem para todos os processos com ID maior
3. **Resposta ANSWER**: Processos com ID maior respondem e iniciam suas próprias eleições
4. **Novo Coordenador**: O processo com maior ID que não recebe resposta se torna o coordenador
5. **Anúncio COORDINATOR**: O novo coordenador anuncia sua eleição para todos

### Tipos de Mensagens

- **ELECTION**: Inicia processo de eleição
- **ANSWER**: Resposta de processos com ID maior
- **COORDINATOR**: Anúncio do novo coordenador

## Compilação e Execução

### Pré-requisitos

```bash
# Ubuntu/Debian
sudo apt-get install mpich

# Fedora/RHEL
sudo dnf install mpich mpich-devel

# Arch Linux
sudo pacman -S openmpi
```

### Compilar

```bash
make
```

Ou manualmente:
```bash
mpicc -o bully_algorithm bully_algorithm.c
```

### Executar

```bash
# Executar com 5 processos
make run

# Ou manualmente com N processos
mpirun -np 5 ./bully_algorithm
```

## Como Funciona

1. O processo com maior ID (N-1) inicia como coordenador
2. Após inicialização, o processo 0 simula a detecção de falha
3. O processo 0 inicia uma eleição enviando mensagens ELECTION
4. Processos com ID maior respondem e iniciam suas próprias eleições
5. O processo com maior ID ativo se torna o novo coordenador

## Exemplo de Saída

```
Processo 0 iniciado
Processo 1 iniciado
Processo 2 iniciado
Processo 3 iniciado
Processo 4 iniciado

>>> Processo 4 é o COORDENADOR inicial <<<

=== Processo 0 detectou falha do coordenador, iniciando eleição ===

Processo 0 iniciando eleição
Processo 0 enviando ELECTION para processo 1
Processo 0 enviando ELECTION para processo 2
Processo 1 recebeu ELECTION do processo 0
Processo 1 enviando ANSWER para processo 0
...

>>> Processo 4 é o novo COORDENADOR <<<

=== ELEIÇÃO CONCLUÍDA ===
Coordenador final: 4
```

## Parâmetros Configuráveis

No código `bully_algorithm.c`:

- `timeout`: Tempo de espera por respostas (padrão: 2 segundos)
- Número de processos: Altere o parâmetro `-np` no comando `mpirun`

## Limpeza

```bash
make clean
```
