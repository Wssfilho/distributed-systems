// Inclui biblioteca MPI e utilitários usados em toda a simulação
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Tags distintas para separar os tipos de mensagem do algoritmo do valentão
#define TAG_ELEICAO 1
#define TAG_OK 2
#define TAG_COORDENADOR 3

// Tempo máximo de espera por respostas OK ou pelo anúncio do coordenador
#define TEMPO_ESPERA_OK 2.0
#define TEMPO_ESPERA_COORD 4.0

// Estados simples que descrevem em que etapa da eleição um processo está
enum
{
    ESTADO_OCIOSO,
    ESTADO_ESPERANDO_OK,
    ESTADO_ESPERANDO_COORD,
    ESTADO_LIDER
};

// Identificação do processo, do tamanho do grupo e do coordenador atual
int rank_mpi;
int total_processos;
int id_processo;
int coordenador = -1;
int estado_eleicao = ESTADO_OCIOSO;

// Parâmetros do cenário: quem detecta falha, quem falha e tempos envolvidos
int processo_detector = -1;
int processo_falho = -1;
double tempo_queda = 2.0;
double tempo_retorno = 9.0;

// Flags que controlam queda/retorno e variáveis de apoio à eleição
int esta_offline = 0;
int ja_caiu = 0;
int ja_voltou = 0;
int recebeu_ok = 0;
double inicio_eleicao = 0.0;
double inicio_espera_coord = 0.0;

// Envia anúncio de coordenação para todo o cluster
static void anunciar_coordenador(void)
{
    coordenador = id_processo;
    estado_eleicao = ESTADO_LIDER;
    printf("\n>>> Processo %d é o novo COORDENADOR <<<\n", id_processo);

    for (int destino = 0; destino < total_processos; destino++)
    {
        if (destino == rank_mpi)
        {
            continue;
        }
        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_COORDENADOR, MPI_COMM_WORLD);
    }
}

// Dispara a eleição seguindo os passos do valentão
static void iniciar_eleicao(const char *motivo, int forcar)
{
    if (esta_offline)
    {
        // Processo caído não participa de eleições
        return;
    }
    if (!forcar && (estado_eleicao == ESTADO_ESPERANDO_OK || estado_eleicao == ESTADO_ESPERANDO_COORD))
    {
        // Já existe eleição em andamento, então evita duplicidade
        return;
    }

    printf("Processo %d iniciando eleição%s\n", id_processo, motivo ? motivo : "");
    recebeu_ok = 0;
    inicio_eleicao = MPI_Wtime();

    int enviou = 0;
    for (int destino = rank_mpi + 1; destino < total_processos; ++destino)
    {
        int id_destino = destino + 1;
        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_ELEICAO, MPI_COMM_WORLD);
        printf("Processo %d -> ELEIÇÃO -> processo %d\n", id_processo, id_destino);
        enviou = 1;
    }

    if (!enviou)
    {
        // Ninguém acima respondeu: este processo assume coordenação
        anunciar_coordenador();
    }
    else
    {
        estado_eleicao = ESTADO_ESPERANDO_OK;
    }
}

// Responde pedidos de eleição vindos de processos menores
static void responder_eleicao(int rank_origem)
{
    int id_origem = rank_origem + 1;
    printf("Processo %d recebeu ELEIÇÃO de %d\n", id_processo, id_origem);
    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD);
    if (estado_eleicao == ESTADO_OCIOSO)
    {
        iniciar_eleicao(" (atendeu pedido)", 0);
    }
}

// Armazena a resposta OK e passa a aguardar o novo coordenador
static void receber_ok(int rank_origem)
{
    printf("Processo %d recebeu OK de %d\n", id_processo, rank_origem + 1);
    recebeu_ok = 1;
    estado_eleicao = ESTADO_ESPERANDO_COORD;
    inicio_espera_coord = MPI_Wtime();
}

// Atualiza o coordenador local após anúncio
static void receber_coordenador(int novo)
{
    coordenador = novo;
    estado_eleicao = (id_processo == novo) ? ESTADO_LIDER : ESTADO_OCIOSO;
    printf("Processo %d reconhece %d como coordenador\n", id_processo, novo);
}

// Varre a fila de mensagens MPI tratando cada tipo de pacote
static void processar_mensagens(void)
{
    while (1)
    {
        int tem_msg;
        MPI_Status status;
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status);
        if (!tem_msg)
        {
            return;
        }

        int payload = -1;
        MPI_Recv(&payload, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

        if (esta_offline)
        {
            if (status.MPI_TAG == TAG_ELEICAO)
            {
                printf("Processo %d offline ignorou ELEIÇÃO de %d\n", id_processo, status.MPI_SOURCE + 1);
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
        default:
            break;
        }
    }
}

// Reinicia a eleição se o processo ficou esperando demais
static void verificar_timeouts(void)
{
    if (esta_offline)
    {
        // Processo offline só registra quem tentou chamá-lo
        return;
    }

    double agora = MPI_Wtime();
    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok && (agora - inicio_eleicao) >= TEMPO_ESPERA_OK)
    {
        printf("Processo %d não recebeu OK; assume liderança\n", id_processo);
        anunciar_coordenador();
    }
    if (estado_eleicao == ESTADO_ESPERANDO_COORD && (agora - inicio_espera_coord) >= TEMPO_ESPERA_COORD)
    {
        printf("Processo %d não recebeu anúncio; reinicia eleição\n", id_processo);
        estado_eleicao = ESTADO_OCIOSO;
        iniciar_eleicao(" (timeout)", 1);
    }
}

// Lê parâmetros da linha de comando e monta o cenário solicitado
static void configurar_cenario(int argc, char **argv)
{

    if (argc > 1)
    {
        processo_falho = atoi(argv[1]);
    }
    if (argc > 2)
    {
        processo_detector = atoi(argv[2]);
    }
    if (argc > 3)
    {
        tempo_queda = atof(argv[3]);
    }
    if (argc > 4)
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
        // Garante que o retorno ocorra em um instante posterior à queda
        tempo_retorno = tempo_queda + 3.0;
    }

    if (rank_mpi == 0)
    {
        printf("Configuração: falho=%d detector=%d queda=%.1fs retorno=%.1fs\n",
               processo_falho, processo_detector, tempo_queda, tempo_retorno);
    }
}

// Programa principal: inicializa MPI e executa a simulação
int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos);

    id_processo = rank_mpi + 1;
    configurar_cenario(argc, argv);

    printf("Processo %d inicializado\n", id_processo);
    MPI_Barrier(MPI_COMM_WORLD);

    if (id_processo == total_processos)
    {
        // Maior ID vira coordenador inicial
        anunciar_coordenador();
    }
    else
    {
        // Os demais aguardam o anúncio inicial
        int msg_coord;
        MPI_Status status;
        MPI_Recv(&msg_coord, 1, MPI_INT, MPI_ANY_SOURCE, TAG_COORDENADOR, MPI_COMM_WORLD, &status);
        receber_coordenador(msg_coord);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double inicio = MPI_Wtime();
    int detector_disparado = 0;
    const double DURACAO = 12.0;

    while (MPI_Wtime() - inicio < DURACAO)
    {
        processar_mensagens();
        verificar_timeouts();

        double decorrido = MPI_Wtime() - inicio;

        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda)
        {
            // Simula a queda do coordenador "valentão"
            esta_offline = 1;
            ja_caiu = 1;
            estado_eleicao = ESTADO_OCIOSO;
            printf("\n--- Processo %d caiu ---\n", id_processo);
        }

        if (!detector_disparado && processo_detector == id_processo && decorrido >= tempo_queda + 1.0)
        {
            // Processo detector percebe o silêncio do coordenador
            detector_disparado = 1;
            printf("\n=== Processo %d detectou falha do coordenador %d ===\n", id_processo, coordenador);
            iniciar_eleicao(" (detecção)", 0);
        }

        if (processo_falho == id_processo && esta_offline && !ja_voltou && decorrido >= tempo_retorno)
        {
            // Processo que havia caído retorna e força nova eleição
            esta_offline = 0;
            ja_voltou = 1;
            printf("\n=== Processo %d voltou e convoca eleição ===\n", id_processo);
            iniciar_eleicao(" (retorno)", 1);
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
