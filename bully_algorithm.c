// Bibliotecas necessárias para comunicação MPI e funções básicas
#include <mpi.h>    // Biblioteca para comunicação entre processos distribuídos
#include <stdio.h>  // Funções de entrada/saída (printf)
#include <stdlib.h> // Funções de conversão (atoi) e controle (exit)
#include <unistd.h> // Função usleep para pausas na execução

// Tags que identificam os tipos de mensagens MPI trocadas entre processos
#define TAG_ELEICAO 1     // Tag para mensagens de início de eleição
#define TAG_OK 2          // Tag para respostas OK (processo vivo com ID maior)
#define TAG_COORDENADOR 3 // Tag para anúncios de novo coordenador

// Tempos de espera para timeouts do algoritmo
#define TEMPO_ESPERA_OK 2.0    // Segundos aguardando respostas OK de processos maiores
#define TEMPO_ESPERA_COORD 4.0 // Segundos aguardando anúncio do novo coordenador

// Enumeração dos estados possíveis de um processo durante a eleição
typedef enum
{
    ESTADO_OCIOSO,          // Processo não está participando de eleição
    ESTADO_ESPERANDO_OK,    // Processo aguardando respostas OK de processos maiores
    ESTADO_ESPERANDO_COORD, // Processo aguardando anúncio de quem é o coordenador
    ESTADO_LIDER            // Processo é o coordenador atual
} Estado;

// Variáveis globais que armazenam informações do processo MPI
int rank_mpi;        // Rank do processo no comunicador MPI (0 a N-1)
int total_processos; // Número total de processos na execução
int id_processo;     // ID do processo para exibição (rank + 1, de 1 a N)

// Variáveis de controle da eleição
int coordenador = -1;                  // ID do coordenador atual (-1 = nenhum)
Estado estado_eleicao = ESTADO_OCIOSO; // Estado atual do processo na eleição

// Parâmetros do cenário de simulação (configurados via linha de comando)
int processo_detector = -1; // ID do processo que detecta falha e inicia eleição
int processo_falho = -1;    // ID do processo que irá cair (simular falha)
int processo_volta = 1;     // Flag: 1 = processo falho volta, 0 = não volta

// Tempos de simulação
double tempo_queda = 2.0;   // Tempo (segundos) até o processo falhar
double tempo_retorno = 9.0; // Tempo (segundos) até o processo voltar

// Flags de controle do estado do processo e da eleição
int esta_offline = 0; // Flag: 1 = processo está offline, 0 = online
int ja_caiu = 0;      // Flag: 1 = processo já executou a queda
int ja_voltou = 0;    // Flag: 1 = processo já retornou após queda
int recebeu_ok = 0;   // Flag: 1 = recebeu pelo menos um OK na eleição atual

// Marcadores de tempo para controle de timeouts
double inicio_eleicao = 0.0;      // Timestamp do início da eleição atual
double inicio_espera_coord = 0.0; // Timestamp do início da espera por coordenador

// Função que anuncia este processo como novo coordenador para todos
void anunciar_coordenador(void)
{
    coordenador = id_processo;                                           // Define este processo como coordenador local
    estado_eleicao = ESTADO_LIDER;                                       // Atualiza estado para líder
    printf("\n>>> Processo %d é o novo COORDENADOR <<<\n", id_processo); // Exibe mensagem

    // Envia mensagem de coordenador para todos os outros processos
    for (int i = 0; i < total_processos; i++)
    {
        if (i != rank_mpi) // Não envia mensagem para si mesmo
        {
            // Envia o ID deste processo com tag TAG_COORDENADOR para o processo i
            MPI_Send(&id_processo, 1, MPI_INT, i, TAG_COORDENADOR, MPI_COMM_WORLD);
        }
    }
}

// Função que inicia uma eleição enviando mensagens para processos com ID maior
// Parâmetros: motivo - string descritiva, forcar - ignora se já está em eleição
void iniciar_eleicao(const char *motivo, int forcar)
{
    // Verifica se pode iniciar eleição
    if (esta_offline || (!forcar && (estado_eleicao == ESTADO_ESPERANDO_OK ||
                                     estado_eleicao == ESTADO_ESPERANDO_COORD)))
    {
        return; // Processo offline ou já em eleição não inicia nova
    }

    // Exibe mensagem informando início da eleição e o motivo
    printf("Processo %d iniciando eleição%s\n", id_processo, motivo ? motivo : "");
    recebeu_ok = 0;               // Reseta flag de recebimento de OK
    inicio_eleicao = MPI_Wtime(); // Marca timestamp do início da eleição

    int enviou = 0; // Flag para verificar se enviou mensagens
    // Envia mensagem de ELEIÇÃO para todos os processos com rank maior
    for (int i = rank_mpi + 1; i < total_processos; i++)
    {
        // Envia mensagem de eleição para o processo i
        MPI_Send(&id_processo, 1, MPI_INT, i, TAG_ELEICAO, MPI_COMM_WORLD);
        printf("Processo %d -> ELEIÇÃO -> processo %d\n", id_processo, i);
        enviou = 1; // Marca que pelo menos uma mensagem foi enviada
    }

    // Se enviou mensagens, aguarda respostas OK
    if (enviou)
    {
        estado_eleicao = ESTADO_ESPERANDO_OK; // Muda estado para esperando OK
    }
    else // Se não há processos maiores
    {
        anunciar_coordenador(); // Este processo se torna coordenador
        // return;
    }
}

// Função chamada quando este processo recebe uma mensagem de ELEIÇÃO
// Parâmetro: rank_origem - rank MPI do processo que enviou a eleição
void responder_eleicao(int rank_origem)
{
    // Exibe mensagem indicando recebimento de eleição
    printf("Processo %d recebeu ELEIÇÃO de %d\n", id_processo, rank_origem);

    // Envia resposta OK para o processo que iniciou a eleição
    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD);

    // Se estava ocioso, inicia sua própria eleição (desafia para coordenação)
    if (estado_eleicao == ESTADO_OCIOSO)
    {
        iniciar_eleicao(" (atendeu pedido)", 0); // Inicia eleição não forçada
    }
}

// Função chamada quando este processo recebe uma resposta OK
// Parâmetro: rank_origem - rank MPI do processo que enviou o OK
void receber_ok(int rank_origem)
{
    // Exibe mensagem indicando recebimento de OK
    printf("Processo %d recebeu OK de %d\n", id_processo, rank_origem);
    recebeu_ok = 1; // Marca flag indicando que recebeu pelo menos um OK

    // Se estava esperando OK, agora passa a esperar anúncio do coordenador
    if (estado_eleicao == ESTADO_ESPERANDO_OK)
    {
        estado_eleicao = ESTADO_ESPERANDO_COORD; // Muda estado para esperando coordenador
        inicio_espera_coord = MPI_Wtime();       // Marca timestamp do início da espera
    }
}

// Função chamada quando este processo recebe anúncio de novo coordenador
// Parâmetro: novo - ID do processo que se tornou coordenador
void receber_coordenador(int novo)
{
    coordenador = novo; // Atualiza variável com o ID do novo coordenador

    // Se o novo coordenador é este processo, vira líder; caso contrário, volta ao estado ocioso
    estado_eleicao = (id_processo == novo) ? ESTADO_LIDER : ESTADO_OCIOSO;

    // Exibe mensagem reconhecendo o novo coordenador
    printf("Processo %d reconhece %d como coordenador\n", id_processo, novo);
}

// Função que processa todas as mensagens MPI pendentes na fila
void processar_mensagens(void)
{
    int tem_msg;       // Flag indicando se há mensagem disponível
    MPI_Status status; // Estrutura com informações sobre a mensagem recebida

    while (1) // Loop contínuo até não haver mais mensagens
    {
        // Verifica se há mensagem disponível sem bloquear (não-bloqueante)
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status);

        if (!tem_msg) // Se não há mensagens pendentes
            return;   // Retorna da função

        int payload; // Variável para armazenar o conteúdo da mensagem
        // Recebe a mensagem de forma bloqueante
        MPI_Recv(&payload, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG,
                 MPI_COMM_WORLD, &status);

        // Se o processo está offline, ignora processamento (exceto log)
        if (esta_offline)
        {
            // Se recebeu eleição, apenas loga que ignorou
            if (status.MPI_TAG == TAG_ELEICAO)
            {
                printf("Processo %d offline ignorou ELEIÇÃO de %d\n",
                       id_processo, status.MPI_SOURCE);
            }
            continue; // Pula para próxima iteração sem processar
        }

        // Processa mensagem de acordo com sua tag
        switch (status.MPI_TAG)
        {
        case TAG_ELEICAO:                         // Mensagem de eleição recebida
            responder_eleicao(status.MPI_SOURCE); // Responde com OK
            break;
        case TAG_OK:                       // Mensagem OK recebida
            receber_ok(status.MPI_SOURCE); // Processa OK
            break;
        case TAG_COORDENADOR:             // Mensagem de anúncio de coordenador
            receber_coordenador(payload); // Atualiza coordenador
            break;
        }
    }
}

// Função que verifica se ocorreram timeouts e toma ações apropriadas
void verificar_timeouts(void)
{
    if (esta_offline) // Se processo está offline
        return;       // Não verifica timeouts

    double agora = MPI_Wtime(); // Obtém timestamp atual

    // Verifica timeout de espera por OK
    // Se estava esperando OK, não recebeu nenhum e o tempo expirou
    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok &&
        (agora - inicio_eleicao) >= TEMPO_ESPERA_OK)
    {
        // Nenhum processo maior respondeu, então este se torna coordenador
        printf("Processo %d não recebeu OK; assume liderança\n", id_processo);
        anunciar_coordenador(); // Anuncia-se como coordenador
    }

    // Verifica timeout de espera por anúncio de coordenador
    // Se estava esperando coordenador e o tempo expirou
    if (estado_eleicao == ESTADO_ESPERANDO_COORD &&
        (agora - inicio_espera_coord) >= TEMPO_ESPERA_COORD)
    {
        // Coordenador não se anunciou, reinicia eleição
        printf("Processo %d não recebeu anúncio; reinicia eleição\n", id_processo);
        estado_eleicao = ESTADO_OCIOSO;   // Volta ao estado ocioso
        iniciar_eleicao(" (timeout)", 1); // Inicia eleição forçada
    }
}

// Função que processa argumentos da linha de comando e configura o cenário
// Parâmetros: argc - número de argumentos, argv - vetor de strings com argumentos
void configurar_cenario(int argc, char **argv)
{
    // Verifica se foram fornecidos argumentos suficientes (mínimo 2)
    if (argc < 3)
    {
        if (rank_mpi == 0) // Apenas processo 0 exibe mensagem de uso
        {
            // Exibe instruções de uso do programa
            printf("Uso: mpirun -np <N> %s <processo_offline> <processo_detector> [processo_volta]\n", argv[0]);
            printf("  processo_offline: ID do processo que cairá (0 a N-1)\n");
            printf("  processo_detector: ID que detecta e inicia eleição (0 a N-1)\n");
            printf("  processo_volta: (opcional) 1=volta, 0=não volta (padrão: 1)\n");
        }
        MPI_Finalize(); // Finaliza ambiente MPI
        exit(1);        // Encerra programa com erro
    }

    // Lê os argumentos obrigatórios
    processo_falho = atoi(argv[1]);    // Converte string para int: processo que cairá
    processo_detector = atoi(argv[2]); // Converte string para int: processo detector

    // Lê argumento opcional (processo volta ou não)
    if (argc > 3)
        processo_volta = (atoi(argv[3]) == 1); // Se argv[3]==1, volta; senão, não volta

    // Valida os parâmetros fornecidos
    if (processo_falho < 0 || processo_falho >= total_processos ||       // Processo falho fora do intervalo
        processo_detector < 0 || processo_detector >= total_processos || // Detector fora do intervalo
        processo_detector == processo_falho)                             // Detector não pode ser o processo que cai
    {
        if (rank_mpi == 0) // Apenas processo 0 exibe erro
        {
            printf("Erro: parâmetros inválidos\n");
        }
        MPI_Finalize(); // Finaliza ambiente MPI
        exit(1);        // Encerra programa com erro
    }

    // Exibe configuração do cenário (apenas processo 0)
    if (rank_mpi == 0)
    {
        printf("=== Configuração ===\n");
        // Exibe parâmetros principais em uma linha
        printf("Processos: %d | Offline: %d | Detector: %d | Volta: %s\n",
               total_processos, processo_falho, processo_detector,
               processo_volta ? "SIM" : "NÃO");
        // Exibe tempos de simulação
        printf("Tempos: queda=%.1fs%s\n", tempo_queda,
               processo_volta ? " | retorno=9.0s" : "");
        printf("====================\n\n");
    }
}

// Função principal do programa
int main(int argc, char **argv)
{
    // Inicializa o ambiente MPI
    MPI_Init(&argc, &argv);
    // Obtém o rank (identificador) deste processo no comunicador
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);
    // Obtém o número total de processos em execução
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos);

    // ID do processo é o próprio rank (0 a N-1)
    id_processo = rank_mpi;
    // Configura cenário baseado nos argumentos da linha de comando
    configurar_cenario(argc, argv);

    // Exibe mensagem de inicialização do processo
    printf("Processo %d inicializado\n", id_processo);
    // Sincroniza todos os processos antes de iniciar simulação
    MPI_Barrier(MPI_COMM_WORLD);

    // Marca o timestamp do início da simulação
    double inicio = MPI_Wtime();
    // Flag para garantir que detector só dispara uma vez
    int detector_disparado = 0;

    // Loop principal da simulação (12 segundos de duração)
    while (MPI_Wtime() - inicio < 12.0)
    {
        // Processa todas as mensagens MPI pendentes
        processar_mensagens();
        // Verifica se ocorreram timeouts e age apropriadamente
        verificar_timeouts();

        // Calcula tempo decorrido desde o início da simulação
        double decorrido = MPI_Wtime() - inicio;

        // Simula queda do processo designado no tempo configurado
        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda)
        {
            esta_offline = 1;               // Marca processo como offline
            ja_caiu = 1;                    // Evita cair novamente
            estado_eleicao = ESTADO_OCIOSO; // Reseta estado da eleição
            printf("\n--- Processo %d caiu (offline) ---\n", id_processo);
        }

        // Detector percebe a queda e inicia eleição (0.5s após a queda)
        if (!detector_disparado && processo_detector == id_processo &&
            decorrido >= tempo_queda + 0.5)
        {
            detector_disparado = 1; // Evita disparar múltiplas vezes
            // Exibe mensagem de detecção
            printf("\n=== Processo %d detectou que processo coordenador %d não responde ===\n",
                   id_processo, total_processos - 1);
            // Inicia eleição forçada
            iniciar_eleicao(" (detectou queda)", 1);
        }

        // Simula retorno do processo (se configurado para voltar)
        if (processo_volta && processo_falho == id_processo && esta_offline &&
            !ja_voltou && decorrido >= tempo_retorno)
        {
            esta_offline = 0; // Marca processo como online novamente
            ja_voltou = 1;    // Evita voltar múltiplas vezes
            printf("\n=== Processo %d voltou e convoca eleição ===\n", id_processo);
            // Inicia eleição forçada após retornar
            iniciar_eleicao(" (retorno)", 1);
        }

        // Pausa de 20ms para não sobrecarregar CPU e terminal
        usleep(20000);
    }

    // Sincroniza todos os processos antes de finalizar
    MPI_Barrier(MPI_COMM_WORLD);
    // Apenas processo 0 exibe cabeçalho de finalização
    if (rank_mpi == 0)
        printf("\n=== ELEIÇÃO CONCLUÍDA ===\n");
    // Cada processo exibe seu coordenador final
    printf("Processo %d - Coordenador final: %d\n", id_processo, coordenador);

    // Finaliza o ambiente MPI
    MPI_Finalize();
    return 0; // Retorna sucesso
}
