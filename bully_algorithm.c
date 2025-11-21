#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define TAG_ELEICAO 1
#define TAG_OK 2
#define TAG_COORDENADOR 3
#define TEMPO_ESPERA_OK 2.0
#define TEMPO_ESPERA_COORD 4.0

typedef enum
{
    ESTADO_OCIOSO,
    ESTADO_ESPERANDO_OK,
    ESTADO_ESPERANDO_COORD,
    ESTADO_LIDER
} Estado;

// Variáveis globais
int rank_mpi, total_processos, id_processo;
int coordenador = -1;
Estado estado_eleicao = ESTADO_OCIOSO;

int processo_detector = -1, processo_falho = -1, processo_volta = 1;
double tempo_queda = 2.0, tempo_retorno = 9.0;

int esta_offline = 0, ja_caiu = 0, ja_voltou = 0, recebeu_ok = 0;
double inicio_eleicao = 0.0, inicio_espera_coord = 0.0;

void anunciar_coordenador(void)
{
    coordenador = id_processo;
    estado_eleicao = ESTADO_LIDER;
    printf("\n>>> Processo %d é o novo COORDENADOR <<<\n", id_processo);

    for (int i = 0; i < total_processos; i++)
    {
        if (i != rank_mpi)
        {
            MPI_Send(&id_processo, 1, MPI_INT, i, TAG_COORDENADOR, MPI_COMM_WORLD);
        }
    }
}

void iniciar_eleicao(const char *motivo, int forcar)
{
    if (esta_offline || (!forcar && (estado_eleicao == ESTADO_ESPERANDO_OK ||
                                     estado_eleicao == ESTADO_ESPERANDO_COORD)))
    {
        return;
    }

    printf("Processo %d iniciando eleição%s\n", id_processo, motivo ? motivo : "");
    recebeu_ok = 0;
    inicio_eleicao = MPI_Wtime();

    int enviou = 0;
    for (int i = rank_mpi + 1; i < total_processos; i++)
    {
        MPI_Send(&id_processo, 1, MPI_INT, i, TAG_ELEICAO, MPI_COMM_WORLD);
        printf("Processo %d -> ELEIÇÃO -> processo %d\n", id_processo, i + 1);
        enviou = 1;
    }

    if (enviou)
    {
        estado_eleicao = ESTADO_ESPERANDO_OK;
    }
    else
    {
        anunciar_coordenador();
    }
}

void responder_eleicao(int rank_origem)
{
    printf("Processo %d recebeu ELEIÇÃO de %d\n", id_processo, rank_origem + 1);
    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD);

    if (estado_eleicao == ESTADO_OCIOSO)
    {
        iniciar_eleicao(" (atendeu pedido)", 0);
    }
}

void receber_ok(int rank_origem)
{
    printf("Processo %d recebeu OK de %d\n", id_processo, rank_origem + 1);
    recebeu_ok = 1;

    if (estado_eleicao == ESTADO_ESPERANDO_OK)
    {
        estado_eleicao = ESTADO_ESPERANDO_COORD;
        inicio_espera_coord = MPI_Wtime();
    }
}

void receber_coordenador(int novo)
{
    coordenador = novo;
    estado_eleicao = (id_processo == novo) ? ESTADO_LIDER : ESTADO_OCIOSO;
    printf("Processo %d reconhece %d como coordenador\n", id_processo, novo);
}

void processar_mensagens(void)
{
    int tem_msg;
    MPI_Status status;

    while (1)
    {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status);
        if (!tem_msg)
            return;

        int payload;
        MPI_Recv(&payload, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG,
                 MPI_COMM_WORLD, &status);

        if (esta_offline)
        {
            if (status.MPI_TAG == TAG_ELEICAO)
            {
                printf("Processo %d offline ignorou ELEIÇÃO de %d\n",
                       id_processo, status.MPI_SOURCE + 1);
            }
            continue;
        }

        switch (status.MPI_TAG)
        {
        case TAG_ELEICAO:
            responder_eleicao(status.MPI_SOURCE);
            break;
        case TAG_OK:
            receber_ok(status.MPI_SOURCE);
            break;
        case TAG_COORDENADOR:
            receber_coordenador(payload);
            break;
        }
    }
}

void verificar_timeouts(void)
{
    if (esta_offline)
        return;

    double agora = MPI_Wtime();

    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok &&
        (agora - inicio_eleicao) >= TEMPO_ESPERA_OK)
    {
        printf("Processo %d não recebeu OK; assume liderança\n", id_processo);
        anunciar_coordenador();
    }

    if (estado_eleicao == ESTADO_ESPERANDO_COORD &&
        (agora - inicio_espera_coord) >= TEMPO_ESPERA_COORD)
    {
        printf("Processo %d não recebeu anúncio; reinicia eleição\n", id_processo);
        estado_eleicao = ESTADO_OCIOSO;
        iniciar_eleicao(" (timeout)", 1);
    }
}

void configurar_cenario(int argc, char **argv)
{
    if (argc < 3)
    {
        if (rank_mpi == 0)
        {
            printf("Uso: mpirun -np <N> %s <processo_offline> <processo_detector> [processo_volta]\n", argv[0]);
            printf("  processo_offline: ID do processo que cairá (1 a N)\n");
            printf("  processo_detector: ID que detecta e inicia eleição (1 a N)\n");
            printf("  processo_volta: (opcional) 1=volta, 0=não volta (padrão: 1)\n");
        }
        MPI_Finalize();
        exit(1);
    }

    processo_falho = atoi(argv[1]);
    processo_detector = atoi(argv[2]);
    if (argc > 3)
        processo_volta = (atoi(argv[3]) == 1);

    if (processo_falho < 1 || processo_falho > total_processos ||
        processo_detector < 1 || processo_detector > total_processos ||
        processo_detector == processo_falho)
    {
        if (rank_mpi == 0)
        {
            printf("Erro: parâmetros inválidos\n");
        }
        MPI_Finalize();
        exit(1);
    }

    if (rank_mpi == 0)
    {
        printf("=== Configuração ===\n");
        printf("Processos: %d | Offline: %d | Detector: %d | Volta: %s\n",
               total_processos, processo_falho, processo_detector,
               processo_volta ? "SIM" : "NÃO");
        printf("Tempos: queda=%.1fs%s\n", tempo_queda,
               processo_volta ? " | retorno=9.0s" : "");
        printf("====================\n\n");
    }
}

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos);

    id_processo = rank_mpi + 1;
    configurar_cenario(argc, argv);

    printf("Processo %d inicializado (sem coordenador)\n", id_processo);
    MPI_Barrier(MPI_COMM_WORLD);

    double inicio = MPI_Wtime();
    int detector_disparado = 0;

    while (MPI_Wtime() - inicio < 12.0)
    {
        processar_mensagens();
        verificar_timeouts();

        double decorrido = MPI_Wtime() - inicio;

        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda)
        {
            esta_offline = 1;
            ja_caiu = 1;
            estado_eleicao = ESTADO_OCIOSO;
            printf("\n--- Processo %d caiu (offline) ---\n", id_processo);
        }

        if (!detector_disparado && processo_detector == id_processo &&
            decorrido >= tempo_queda + 0.5)
        {
            detector_disparado = 1;
            printf("\n=== Processo %d detectou que processo %d caiu ===\n",
                   id_processo, processo_falho);
            iniciar_eleicao(" (detectou queda)", 1);
        }

        if (processo_volta && processo_falho == id_processo && esta_offline &&
            !ja_voltou && decorrido >= tempo_retorno)
        {
            esta_offline = 0;
            ja_voltou = 1;
            printf("\n=== Processo %d voltou e convoca eleição ===\n", id_processo);
            iniciar_eleicao(" (retorno)", 1);
        }

        usleep(20000);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank_mpi == 0)
        printf("\n=== ELEIÇÃO CONCLUÍDA ===\n");
    printf("Processo %d - Coordenador final: %d\n", id_processo, coordenador);

    MPI_Finalize();
    return 0;
}
