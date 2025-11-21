// Inclui biblioteca MPI para comunicação entre processos distribuídos
#include <mpi.h> // Biblioteca para comunicação paralela
// Inclui biblioteca padrão de entrada/saída
#include <stdio.h> // Funções de entrada e saída
// Inclui biblioteca padrão para funções gerais (malloc, exit, etc)
#include <stdlib.h> // Funções utilitárias e conversão
// Inclui biblioteca para funções POSIX (sleep, usleep)
#include <unistd.h> // Funções de espera
// Inclui biblioteca para funções de tempo
#include <time.h> // Funções de tempo

// Define a tag para mensagens de ELEIÇÃO
#define TAG_ELEICAO 1 // Tag para mensagem de eleição
// Define a tag para mensagens de resposta OK
#define TAG_OK 2 // Tag para mensagem OK
// Define a tag para mensagens de anúncio de COORDENADOR
#define TAG_COORDENADOR 3 // Tag para mensagem de coordenador

// Timeout para aguardar respostas OK (em segundos)
#define TEMPO_ESPERA_OK 2.0 // Tempo de espera por OK
// Timeout para aguardar a mensagem do coordenador (em segundos)
#define TEMPO_ESPERA_COORD 4.0 // Tempo de espera pelo coordenador

// Estados possíveis durante a eleição
typedef enum // Enumeração dos estados do processo
{
    ESTADO_OCIOSO,          // Não está em eleição
    ESTADO_ESPERANDO_OK,    // Esperando resposta OK
    ESTADO_ESPERANDO_COORD, // Esperando anúncio do coordenador
    ESTADO_LIDER            // É o coordenador
} EstadoEleicao;            // Nome do tipo

// Variáveis globais compartilhadas entre as funções
int rank_mpi;         // rank MPI (base 0)
int total_processos;  // número total de processos
int id_processo;      // ID lógico (base 1)
int coordenador = -1; // coordenador atual conhecido

EstadoEleicao estado_eleicao = ESTADO_OCIOSO; // Estado inicial
double inicio_eleicao = 0.0;                  // Tempo de início da eleição
double inicio_espera_coordenador = 0.0;       // Tempo de início da espera pelo coordenador
int recebeu_ok = 0;                           // Flag se recebeu OK

// Configuração do cenário descrito por Tanenbaum/Steen
int processo_detector = -1; // Processo que detecta a falha
int processo_falho = -1;    // Processo que vai cair
double tempo_queda = 2.0;   // Tempo para cair
double tempo_retorno = 9.0; // Tempo para retornar
int esta_offline = 0;       // Flag se está offline
int ja_caiu = 0;            // Flag se já caiu
int ja_voltou = 0;          // Flag se já voltou

// Protótipos das funções
void iniciar_eleicao(const char *motivo, int forcar_inicio); // Inicia eleição
void assumir_coordenacao(void);                              // Assume coordenação
void processar_mensagens(void);                              // Processa mensagens
void tratar_elecao(int rank_origem);                         // Trata eleição recebida
void tratar_ok(int rank_origem);                             // Trata OK recebido
void tratar_coordenador(int novo_coordenador);               // Trata anúncio de coordenador
void verificar_timeouts(void);                               // Verifica timeouts

// Inicia uma nova eleição seguindo o algoritmo do valentão
void iniciar_eleicao(const char *motivo, int forcar_inicio) // Função para iniciar eleição
{
    if (esta_offline) // Se está offline, não faz nada
    {
        return; // Sai da função
    }

    if (!forcar_inicio && (estado_eleicao == ESTADO_ESPERANDO_OK || estado_eleicao == ESTADO_ESPERANDO_COORD)) // Se já está em eleição
    {
        return; // Sai da função
    }

    if (motivo) // Se tem motivo, imprime
    {
        printf("Processo %d iniciando eleição (%s)\n", id_processo, motivo); // Log
    }
    else // Senão, imprime sem motivo
    {
        printf("Processo %d iniciando eleição\n", id_processo); // Log
    }

    recebeu_ok = 0;               // Zera flag de OK
    inicio_eleicao = MPI_Wtime(); // Marca início

    int destinos_maiores = 0;                                              // Contador de destinos
    for (int destino = rank_mpi + 1; destino < total_processos; destino++) // Para cada processo maior
    {
        int id_destino = destino + 1;                                                     // ID do destino
        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_ELEICAO, MPI_COMM_WORLD);         // Envia eleição
        printf("Processo %d enviou ELEIÇÃO para processo %d\n", id_processo, id_destino); // Log
        destinos_maiores++;                                                               // Incrementa
    }

    if (destinos_maiores == 0) // Se não há maiores
    {
        assumir_coordenacao(); // Assume coordenação
        return;                // Sai
    }

    estado_eleicao = ESTADO_ESPERANDO_OK; // Atualiza estado
}

// Assume o papel de coordenador e notifica todos os outros processos
void assumir_coordenacao(void) // Função para assumir coordenação
{
    coordenador = id_processo;                                                                // Atualiza coordenador
    estado_eleicao = ESTADO_LIDER;                                                            // Atualiza estado
    printf("\n>>> Processo %d VENCEU a eleição e é o novo COORDENADOR <<<\n\n", id_processo); // Log

    for (int destino = 0; destino < total_processos; destino++) // Para todos os processos
    {
        if (destino == rank_mpi) // Se for ele mesmo
        {
            continue; // Não envia para si
        }

        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_COORDENADOR, MPI_COMM_WORLD); // Envia coordenador
    }
}

// Processa mensagens disponíveis (ELEIÇÃO, OK, COORDENADOR)
void processar_mensagens(void) // Função para processar mensagens
{
    int tem_msg;       // Flag de mensagem
    MPI_Status status; // Status da mensagem

    while (1) // Loop infinito
    {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status); // Verifica mensagem
        if (!tem_msg)                                                               // Se não tem mensagem
        {
            break; // Sai do loop
        }

        int conteudo = -1;                                                                           // Conteúdo da mensagem
        MPI_Recv(&conteudo, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status); // Recebe mensagem

        if (status.MPI_TAG == TAG_ELEICAO) // Se for eleição
        {
            if (esta_offline) // Se está offline
            {
                int id_origem = status.MPI_SOURCE + 1;                                                        // ID origem
                printf("Processo %d está offline e ignora ELEIÇÃO do processo %d\n", id_processo, id_origem); // Log
                continue;                                                                                     // Ignora
            }

            tratar_elecao(status.MPI_SOURCE); // Trata eleição
        }
        else if (status.MPI_TAG == TAG_OK) // Se for OK
        {
            if (esta_offline) // Se está offline
            {
                continue; // Ignora
            }

            tratar_ok(status.MPI_SOURCE); // Trata OK
        }
        else if (status.MPI_TAG == TAG_COORDENADOR) // Se for coordenador
        {
            if (esta_offline) // Se está offline
            {
                continue; // Ignora
            }

            tratar_coordenador(conteudo); // Trata coordenador
        }
    }
}

// Responde a uma mensagem de eleição recebida
void tratar_elecao(int rank_origem) // Função para tratar eleição recebida
{
    int id_origem = rank_origem + 1;                                                // ID origem
    printf("Processo %d recebeu ELEIÇÃO do processo %d\n", id_processo, id_origem); // Log

    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD);     // Envia OK
    printf("Processo %d respondeu OK ao processo %d\n", id_processo, id_origem); // Log

    if (estado_eleicao == ESTADO_OCIOSO) // Se estava ocioso
    {
        iniciar_eleicao("recebeu ELEIÇÃO", 0); // Inicia eleição
    }
}

// Processa uma mensagem OK recebida de um processo com ID maior
void tratar_ok(int rank_origem) // Função para tratar OK recebido
{
    int id_origem = rank_origem + 1;           // ID origem
    if (estado_eleicao != ESTADO_ESPERANDO_OK) // Se não está esperando OK
    {
        return; // Ignora
    }

    printf("Processo %d recebeu OK do processo %d\n", id_processo, id_origem);    // Log
    recebeu_ok = 1;                                                               // Marca que recebeu OK
    estado_eleicao = ESTADO_ESPERANDO_COORD;                                      // Atualiza estado
    inicio_espera_coordenador = MPI_Wtime();                                      // Marca início da espera
    printf("Processo %d aguardará o anúncio do novo coordenador\n", id_processo); // Log
}

// Processa o anúncio do novo coordenador
void tratar_coordenador(int novo_coordenador) // Função para tratar coordenador
{
    coordenador = novo_coordenador;                                                           // Atualiza coordenador
    printf("Processo %d reconhece processo %d como COORDENADOR\n", id_processo, coordenador); // Log

    if (id_processo == coordenador) // Se é o coordenador
    {
        estado_eleicao = ESTADO_LIDER; // Atualiza estado
    }
    else // Senão
    {
        estado_eleicao = ESTADO_OCIOSO; // Atualiza estado
    }
}

// Verifica timeouts de eleição e reinicia se necessário
void verificar_timeouts(void) // Função para verificar timeouts
{
    if (esta_offline) // Se está offline
    {
        return; // Sai
    }

    double agora = MPI_Wtime(); // Tempo atual

    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok && (agora - inicio_eleicao) >= TEMPO_ESPERA_OK) // Timeout OK
    {
        printf("Processo %d não recebeu OK; assume que todos os maiores falharam\n", id_processo); // Log
        assumir_coordenacao();                                                                     // Assume coordenação
    }

    if (estado_eleicao == ESTADO_ESPERANDO_COORD && (agora - inicio_espera_coordenador) >= TEMPO_ESPERA_COORD) // Timeout coordenador
    {
        printf("Processo %d não recebeu anúncio do coordenador; reiniciará a eleição\n", id_processo); // Log
        estado_eleicao = ESTADO_OCIOSO;                                                                // Atualiza estado
        iniciar_eleicao("timeout aguardando coordenador", 1);                                          // Reinicia eleição
    }
}

// Configura o cenário de execução
static void configurar_cenario(int argc, char **argv)
{
    if (argc >= 2) // Se tem argumento de falha
    {
        processo_falho = atoi(argv[1]); // Converte
    }
    if (argc >= 3) // Se tem detector
    {
        processo_detector = atoi(argv[2]); // Converte
    }
    if (argc >= 4) // Se tem tempo de queda
    {
        tempo_queda = atof(argv[3]); // Converte
    }
    if (argc >= 5) // Se tem tempo de retorno
    {
        tempo_retorno = atof(argv[4]); // Converte
    }

    if (processo_falho < 1 || processo_falho > total_processos) // Valida falho
    {
        processo_falho = -1; // Invalida
    }
    if (processo_detector < 1 || processo_detector > total_processos) // Valida detector
    {
        processo_detector = -1; // Invalida
    }
    if (tempo_retorno <= tempo_queda) // Valida tempos
    {
        tempo_retorno = tempo_queda + 3.0; // Corrige
    }

    if (rank_mpi == 0) // Só o rank 0 imprime
    {
        printf("Configuração: processo que cai=%d, detector=%d, queda=%.1fs, retorno=%.1fs\n",
               processo_falho, processo_detector, tempo_queda, tempo_retorno); // Log
    }
}

// Função principal
int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);                          // Inicializa MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);        // Obtém rank
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos); // Obtém total

    id_processo = rank_mpi + 1;                 // Calcula ID
    srand((unsigned int)time(NULL) + rank_mpi); // Inicializa random

    configurar_cenario(argc, argv); // Configura cenário

    printf("Processo %d (rank %d) iniciado\n", id_processo, rank_mpi); // Log

    MPI_Barrier(MPI_COMM_WORLD); // Sincroniza

    // Coordenador inicial é o processo de maior ID lógico
    int coordenador_inicial = total_processos; // Maior ID
    if (id_processo == coordenador_inicial)    // Se é o maior
    {
        coordenador = id_processo;                                                // Atualiza
        estado_eleicao = ESTADO_LIDER;                                            // Atualiza
        printf("\n>>> Processo %d é o COORDENADOR inicial <<<\n\n", id_processo); // Log

        for (int destino = 0; destino < total_processos; destino++) // Para todos
        {
            if (destino == rank_mpi) // Se for ele
            {
                continue; // Pula
            }

            MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_COORDENADOR, MPI_COMM_WORLD); // Envia
        }
    }
    else // Se não é o maior
    {
        int msg_coord;                                                                                    // Mensagem
        MPI_Status status;                                                                                // Status
        MPI_Recv(&msg_coord, 1, MPI_INT, MPI_ANY_SOURCE, TAG_COORDENADOR, MPI_COMM_WORLD, &status);       // Recebe
        coordenador = msg_coord;                                                                          // Atualiza
        printf("Processo %d reconhece processo %d como COORDENADOR inicial\n", id_processo, coordenador); // Log
    }

    MPI_Barrier(MPI_COMM_WORLD); // Sincroniza

    double inicio_simulacao = MPI_Wtime(); // Marca início
    int detector_disparado = 0;            // Flag detector

    while (MPI_Wtime() - inicio_simulacao < 12.0) // Loop principal
    {
        processar_mensagens(); // Processa mensagens
        verificar_timeouts();  // Verifica timeouts

        double decorrido = MPI_Wtime() - inicio_simulacao; // Tempo decorrido

        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda) // Se deve cair
        {
            esta_offline = 1;                                               // Fica offline
            ja_caiu = 1;                                                    // Marca que caiu
            estado_eleicao = ESTADO_OCIOSO;                                 // Atualiza estado
            printf("\n--- Processo %d ficou offline ---\n\n", id_processo); // Log
        }

        if (!detector_disparado && processo_detector == id_processo && decorrido >= tempo_queda + 1.0) // Se deve detectar
        {
            detector_disparado = 1;                                                                                   // Marca
            printf("\n=== Processo %d percebeu que o coordenador %d não responde ===\n\n", id_processo, coordenador); // Log
            iniciar_eleicao("coordenador não responde", 0);                                                           // Inicia eleição
        }

        if (processo_falho == id_processo && esta_offline && !ja_voltou && decorrido >= tempo_retorno) // Se deve voltar
        {
            esta_offline = 0;                                                                          // Fica online
            ja_voltou = 1;                                                                             // Marca que voltou
            printf("\n=== Processo %d voltou ao sistema e convoca nova eleição ===\n\n", id_processo); // Log
            iniciar_eleicao("processo voltou ao sistema", 1);                                          // Inicia eleição
        }

        usleep(20000); // Espera
    }

    MPI_Barrier(MPI_COMM_WORLD); // Sincroniza
    if (rank_mpi == 0)           // Se é o rank 0
    {
        printf("\n=== ELEIÇÃO CONCLUÍDA ===\n"); // Log
    }
    printf("Processo %d - Coordenador final: %d\n", id_processo, coordenador); // Log final

    MPI_Finalize(); // Finaliza MPI
    return 0;       // Fim
}
