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
# Sintaxe: mpirun -np <N> ./bully_algorithm <processo_offline> <processo_detector> [processo_volta]
# Onde:
#   N: número total de processos
#   processo_offline: ID do processo que cairá (1 a N)
#   processo_detector: ID do processo que detectará a queda e iniciará eleição (1 a N)
#   processo_volta: (opcional) 1=processo volta, 0=não volta (padrão: 1)

# Exemplo com 5 processos, processo 3 offline, processo 1 como detector (processo volta)
mpirun -np 5 ./bully_algorithm 3 1 1

# Exemplo com 4 processos, processo 2 offline, processo 4 como detector (processo NÃO volta)
mpirun -np 4 ./bully_algorithm 2 4 0

# Exemplo com padrão (processo volta automaticamente)
mpirun -np 5 ./bully_algorithm 3 1
```

## Como Funciona

1. **Inicialização**: Todos os processos iniciam SEM coordenador
2. **Queda**: O processo especificado como offline cai após 2 segundos
3. **Detecção**: O processo detector percebe a queda e inicia uma eleição
4. **Eleição**: Processo detector envia mensagens ELECTION para processos com ID maior
5. **Respostas**: Processos com ID maior respondem OK e iniciam suas próprias eleições
6. **Coordenador**: O processo com maior ID ativo se torna o novo coordenador
7. **Retorno (opcional)**: Se configurado, o processo que caiu pode retornar após 9 segundos e forçar nova eleição

## Exemplo de Saída

```bash
# Executando: mpirun -np 5 ./bully_algorithm 3 1
=== Configuração ===
Número de processos: 5
Processo offline: 3
Processo detector: 1
Tempo de queda: 2.0s
====================

Processo 1 inicializado (sem coordenador)
Processo 2 inicializado (sem coordenador)
Processo 3 inicializado (sem coordenador)
Processo 4 inicializado (sem coordenador)
Processo 5 inicializado (sem coordenador)

--- Processo 3 caiu (offline) ---

=== Processo 1 detectou que processo 3 caiu - iniciando eleição ===

Processo 1 iniciando eleição (detectou queda)
Processo 1 -> ELEIÇÃO -> processo 2
Processo 1 -> ELEIÇÃO -> processo 4
Processo 1 -> ELEIÇÃO -> processo 5

Processo 2 recebeu ELEIÇÃO de 1
Processo 4 recebeu ELEIÇÃO de 1
Processo 5 recebeu ELEIÇÃO de 1

Processo 1 recebeu OK de 2
Processo 2 iniciando eleição (atendeu pedido)
...

>>> Processo 5 é o novo COORDENADOR <<<

=== ELEIÇÃO CONCLUÍDA ===
Processo 1 - Coordenador final: 5
Processo 2 - Coordenador final: 5
Processo 4 - Coordenador final: 5
Processo 5 - Coordenador final: 5
```

## Parâmetros Configuráveis

No código `bully_algorithm.c`:

- `timeout`: Tempo de espera por respostas (padrão: 2 segundos)
- Número de processos: Altere o parâmetro `-np` no comando `mpirun`

## Limpeza

```bash
make clean
```
