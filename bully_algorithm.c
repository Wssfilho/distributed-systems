// Inclui biblioteca MPI para comunicação entre processos distribuídos
#include <mpi.h>
// Inclui biblioteca padrão de entrada/saída
#include <stdio.h>
// Inclui biblioteca padrão para funções gerais (malloc, exit, etc)
#include <stdlib.h>
// Inclui biblioteca para funções POSIX (sleep, usleep)
#include <unistd.h>
// Inclui biblioteca para funções de tempo
#include <time.h>

// Define a tag para mensagens de ELEIÇÃO
#define TAG_ELEICAO 1
// Define a tag para mensagens de resposta OK
#define TAG_OK 2
// Define a tag para mensagens de anúncio de COORDENADOR
#define TAG_COORDENADOR 3

// Timeout para aguardar respostas OK (em segundos)
#define TEMPO_ESPERA_OK 2.0
// Timeout para aguardar a mensagem do coordenador (em segundos)
#define TEMPO_ESPERA_COORD 4.0

// Estados possíveis durante a eleição
typedef enum
{
    ESTADO_OCIOSO,
    ESTADO_ESPERANDO_OK,
    ESTADO_ESPERANDO_COORD,
    ESTADO_LIDER
} EstadoEleicao;

// Variáveis globais compartilhadas entre as funções
int rank_mpi;         // rank MPI (base 0)
int total_processos;  // número total de processos
int id_processo;      // ID lógico (base 1) para combinar com o relato
int coordenador = -1; // coordenador atual conhecido

EstadoEleicao estado_eleicao = ESTADO_OCIOSO;
double inicio_eleicao = 0.0;
double inicio_espera_coordenador = 0.0;
int recebeu_ok = 0;

// Configuração do cenário descrito por Tanenbaum/Steen
int processo_detector = -1; // processo que detecta a falha do coordenador
int processo_falho = -1;    // processo que "cai" e volta depois
double tempo_queda = 2.0;
double tempo_retorno = 9.0;
int esta_offline = 0;
int ja_caiu = 0;
int ja_voltou = 0;

// Protótipos
void iniciar_eleicao(const char *motivo, int forcar_inicio);
void assumir_coordenacao(void);
void processar_mensagens(void);
void tratar_elecao(int rank_origem);
void tratar_ok(int rank_origem);
void tratar_coordenador(int novo_coordenador);
void verificar_timeouts(void);

// Inicia uma nova eleição seguindo o algoritmo do valentão
void iniciar_eleicao(const char *motivo, int forcar_inicio)
{
    if (esta_offline)
    {
        return;
    }

    if (!forcar_inicio && (estado_eleicao == ESTADO_ESPERANDO_OK || estado_eleicao == ESTADO_ESPERANDO_COORD))
    {
        return;
    }

    if (motivo)
    {
        printf("Processo %d iniciando eleição (%s)\n", id_processo, motivo);
    }
    else
    {
        printf("Processo %d iniciando eleição\n", id_processo);
    }

    recebeu_ok = 0;
    inicio_eleicao = MPI_Wtime();

    int destinos_maiores = 0;
    for (int destino = rank_mpi + 1; destino < total_processos; destino++)
    {
        int id_destino = destino + 1;
        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_ELEICAO, MPI_COMM_WORLD);
        printf("Processo %d enviou ELEIÇÃO para processo %d\n", id_processo, id_destino);
        destinos_maiores++;
    }

    if (destinos_maiores == 0)
    {
        assumir_coordenacao();
        return;
    }

    estado_eleicao = ESTADO_ESPERANDO_OK;
}

// Assume o papel de coordenador e notifica todos os outros processos
void assumir_coordenacao(void)
{
    coordenador = id_processo;
    estado_eleicao = ESTADO_LIDER;
    printf("\n>>> Processo %d VENCEU a eleição e é o novo COORDENADOR <<<\n\n", id_processo);

    for (int destino = 0; destino < total_processos; destino++)
    {
        if (destino == rank_mpi)
        {
            continue;
        }

        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_COORDENADOR, MPI_COMM_WORLD);
    }
}

// Processa mensagens disponíveis (ELEIÇÃO, OK, COORDENADOR)
void processar_mensagens(void)
{
    int tem_msg;
    MPI_Status status;

    while (1)
    {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status);
        if (!tem_msg)
        {
            break;
        }

        int conteudo = -1;
        MPI_Recv(&conteudo, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == TAG_ELEICAO)
        {
            if (esta_offline)
            {
                int id_origem = status.MPI_SOURCE + 1;
                printf("Processo %d está offline e ignora ELEIÇÃO do processo %d\n", id_processo, id_origem);
                continue;
            }

            tratar_elecao(status.MPI_SOURCE);
        }
        else if (status.MPI_TAG == TAG_OK)
        {
            if (esta_offline)
            {
                continue;
            }

            tratar_ok(status.MPI_SOURCE);
        }
        else if (status.MPI_TAG == TAG_COORDENADOR)
        {
            if (esta_offline)
            {
                continue;
            }

            tratar_coordenador(conteudo);
        }
    }
}

// Responde a uma mensagem de eleição recebida
void tratar_elecao(int rank_origem)
{
    int id_origem = rank_origem + 1;
    printf("Processo %d recebeu ELEIÇÃO do processo %d\n", id_processo, id_origem);

    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD);
    printf("Processo %d respondeu OK ao processo %d\n", id_processo, id_origem);

    if (estado_eleicao == ESTADO_OCIOSO)
    {
        iniciar_eleicao("recebeu ELEIÇÃO", 0);
    }
}

// Processa uma mensagem OK recebida de um processo com ID maior
void tratar_ok(int rank_origem)
{
    int id_origem = rank_origem + 1;
    if (estado_eleicao != ESTADO_ESPERANDO_OK)
    {
        return;
    }

    printf("Processo %d recebeu OK do processo %d\n", id_processo, id_origem);
    recebeu_ok = 1;
    estado_eleicao = ESTADO_ESPERANDO_COORD;
    inicio_espera_coordenador = MPI_Wtime();
    printf("Processo %d aguardará o anúncio do novo coordenador\n", id_processo);
}

// Processa o anúncio do novo coordenador
void tratar_coordenador(int novo_coordenador)
{
    coordenador = novo_coordenador;
    printf("Processo %d reconhece processo %d como COORDENADOR\n", id_processo, coordenador);

    if (id_processo == coordenador)
    {
        estado_eleicao = ESTADO_LIDER;
    }
    else
    {
        estado_eleicao = ESTADO_OCIOSO;
    }
}

// Verifica timeouts de eleição e reinicia se necessário
void verificar_timeouts(void)
{
    if (esta_offline)
    {
        return;
    }

    double agora = MPI_Wtime();

    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok && (agora - inicio_eleicao) >= TEMPO_ESPERA_OK)
    {
        printf("Processo %d não recebeu OK; assume que todos os maiores falharam\n", id_processo);
        assumir_coordenacao();
    }

    if (estado_eleicao == ESTADO_ESPERANDO_COORD && (agora - inicio_espera_coordenador) >= TEMPO_ESPERA_COORD)
    {
        printf("Processo %d não recebeu anúncio do coordenador; reiniciará a eleição\n", id_processo);
        estado_eleicao = ESTADO_OCIOSO;
        iniciar_eleicao("timeout aguardando coordenador", 1);
    }
}

static void configurar_cenario(int argc, char **argv)
{
    processo_detector = (total_processos >= 4) ? 4 : total_processos;
    processo_falho = (total_processos >= 7) ? 7 : total_processos;

    if (argc >= 2)
    {
        processo_falho = atoi(argv[1]);
    }
    if (argc >= 3)
    {
        processo_detector = atoi(argv[2]);
    }
    if (argc >= 4)
    {
        tempo_queda = atof(argv[3]);
    }
    if (argc >= 5)
    {
        tempo_retorno = atof(argv[4]);
    }

    if (processo_falho < 1 || processo_falho > total_processos)
    {
        processo_falho = -1;
    }
    if (processo_detector < 1 || processo_detector > total_processos)
    {
        processo_detector = -1;
    }
    if (tempo_retorno <= tempo_queda)
    {
        tempo_retorno = tempo_queda + 3.0;
    }

    if (rank_mpi == 0)
    {
        printf("Configuração: processo que cai=%d, detector=%d, queda=%.1fs, retorno=%.1fs\n",
               processo_falho, processo_detector, tempo_queda, tempo_retorno);
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos);

    id_processo = rank_mpi + 1;
    srand((unsigned int)time(NULL) + rank_mpi);

    configurar_cenario(argc, argv);

    printf("Processo %d (rank %d) iniciado\n", id_processo, rank_mpi);

    MPI_Barrier(MPI_COMM_WORLD);

    // Coordenador inicial é o processo de maior ID lógico
    int coordenador_inicial = total_processos;
    if (id_processo == coordenador_inicial)
    {
        coordenador = id_processo;
        estado_eleicao = ESTADO_LIDER;
        printf("\n>>> Processo %d é o COORDENADOR inicial <<<\n\n", id_processo);

        for (int destino = 0; destino < total_processos; destino++)
        {
            if (destino == rank_mpi)
            {
                continue;
            }

            MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_COORDENADOR, MPI_COMM_WORLD);
        }
    }
    else
    {
        int msg_coord;
        MPI_Status status;
        MPI_Recv(&msg_coord, 1, MPI_INT, MPI_ANY_SOURCE, TAG_COORDENADOR, MPI_COMM_WORLD, &status);
        coordenador = msg_coord;
        printf("Processo %d reconhece processo %d como COORDENADOR inicial\n", id_processo, coordenador);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double inicio_simulacao = MPI_Wtime();
    int detector_disparado = 0;

    while (MPI_Wtime() - inicio_simulacao < 12.0)
    {
        processar_mensagens();
        verificar_timeouts();

        double decorrido = MPI_Wtime() - inicio_simulacao;

        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda)
        {
            esta_offline = 1;
            ja_caiu = 1;
            estado_eleicao = ESTADO_OCIOSO;
            printf("\n--- Processo %d ficou offline ---\n\n", id_processo);
        }

        if (!detector_disparado && processo_detector == id_processo && decorrido >= tempo_queda + 1.0)
        {
            detector_disparado = 1;
            printf("\n=== Processo %d percebeu que o coordenador %d não responde ===\n\n", id_processo, coordenador);
            iniciar_eleicao("coordenador não responde", 0);
        }

        if (processo_falho == id_processo && esta_offline && !ja_voltou && decorrido >= tempo_retorno)
        {
            esta_offline = 0;
            ja_voltou = 1;
            printf("\n=== Processo %d voltou ao sistema e convoca nova eleição ===\n\n", id_processo);
            iniciar_eleicao("processo voltou ao sistema", 1);
        }

        usleep(20000);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank_mpi == 0)
    {
        printf("\n=== ELEIÇÃO CONCLUÍDA ===\n");
    }
    printf("Processo %d - Coordenador final: %d\n", id_processo, coordenador);

    MPI_Finalize();
    return 0;
}
