// Inclui biblioteca MPI e utilitários usados em toda a simulação
#include <mpi.h>    // Biblioteca principal para comunicação distribuída
#include <stdio.h>  // Funções de entrada e saída padrão
#include <stdlib.h> // Conversões numéricas usadas para argumentos
#include <unistd.h> // Funções de temporização simples como usleep

// Tags distintas para separar os tipos de mensagem do algoritmo do valentão
#define TAG_ELEICAO 1     // Tag usada para mensagens de eleição
#define TAG_OK 2          // Tag destinada às respostas OK
#define TAG_COORDENADOR 3 // Tag para anúncios de coordenador

// Tempo máximo de espera por respostas OK ou pelo anúncio do coordenador
#define TEMPO_ESPERA_OK 2.0    // Janela em segundos aguardando respostas OK
#define TEMPO_ESPERA_COORD 4.0 // Janela em segundos aguardando anúncio do líder

// Estados simples que descrevem em que etapa da eleição um processo está
enum
{
    ESTADO_OCIOSO,          // Processo não participa de eleição
    ESTADO_ESPERANDO_OK,    // Processo aguarda respostas OK
    ESTADO_ESPERANDO_COORD, // Processo aguarda anúncio do novo coordenador
    ESTADO_LIDER            // Processo atua como coordenador atual
}; // Fim da enumeração de estados

// Identificação do processo, do tamanho do grupo e do coordenador atual
int rank_mpi;                       // Rank MPI adquirido via communicator
int total_processos;                // Quantidade de processos participantes
int id_processo;                    // Identificador lógico usado nos logs
int coordenador = -1;               // Coordenador reconhecido pelo processo
int estado_eleicao = ESTADO_OCIOSO; // Estado inicial do processo

// Parâmetros do cenário: quem detecta falha, quem falha e tempos envolvidos
int processo_detector = -1; // Processo que detectará o fim do coordenador
int processo_falho = -1;    // Processo escolhido para cair
double tempo_queda = 2.0;   // Momento da queda em segundos
double tempo_retorno = 9.0; // Momento do retorno em segundos

// Flags que controlam queda/retorno e variáveis de apoio à eleição
int esta_offline = 0;             // Indica se o processo está simulado como offline
int ja_caiu = 0;                  // Marca se a queda já foi executada
int ja_voltou = 0;                // Marca se o retorno já aconteceu
int recebeu_ok = 0;               // Flag que armazena se algum OK chegou
double inicio_eleicao = 0.0;      // Instante em que iniciamos a eleição atual
double inicio_espera_coord = 0.0; // Instante em que passamos a esperar o líder

// Envia anúncio de coordenação para todo o cluster
static void anunciar_coordenador(void)
{
    coordenador = id_processo;                                           // Atualiza coordenador local com este processo
    estado_eleicao = ESTADO_LIDER;                                       // Ajusta estado para líder
    printf("\n>>> Processo %d é o novo COORDENADOR <<<\n", id_processo); // Log amigável

    for (int destino = 0; destino < total_processos; destino++) // Percorre todos os ranks
    {
        if (destino == rank_mpi) // Evita enviar mensagem para si mesmo
        {
            continue; // Pula o envio ao próprio processo
        }
        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_COORDENADOR, MPI_COMM_WORLD); // Notifica os demais
    }
} // Fim da função anunciar_coordenador

// Dispara a eleição seguindo os passos do valentão
static void iniciar_eleicao(const char *motivo, int forcar)
{
    if (esta_offline) // Se o processo simulou queda
    {
        // Processo caído não participa de eleições
        return; // Sai imediatamente
    }
    if (!forcar && (estado_eleicao == ESTADO_ESPERANDO_OK || estado_eleicao == ESTADO_ESPERANDO_COORD)) // Verifica se eleição em andamento
    {
        // Já existe eleição em andamento, então evita duplicidade
        return; // Evita iniciar outra eleição
    }

    printf("Processo %d iniciando eleição%s\n", id_processo, motivo ? motivo : ""); // Loga motivo da eleição
    recebeu_ok = 0;                                                                 // Zera flag de OK anterior
    inicio_eleicao = MPI_Wtime();                                                   // Registra o instante de início

    int enviou = 0;                                                        // Indica se enviamos pedidos a processos superiores
    for (int destino = rank_mpi + 1; destino < total_processos; ++destino) // Itera sobre ranks maiores
    {
        int id_destino = destino + 1;                                               // Converte rank em ID humano
        MPI_Send(&id_processo, 1, MPI_INT, destino, TAG_ELEICAO, MPI_COMM_WORLD);   // Envia mensagem de eleição
        printf("Processo %d -> ELEIÇÃO -> processo %d\n", id_processo, id_destino); // Loga envio
        enviou = 1;                                                                 // Marca que ao menos um destino recebeu mensagem
    }

    if (!enviou) // Caso não existam processos maiores
    {
        // Ninguém acima respondeu: este processo assume coordenação
        anunciar_coordenador(); // Assume liderança de imediato
    }
    else // Caso tenha remetido solicitações
    {
        estado_eleicao = ESTADO_ESPERANDO_OK; // Atualiza estado para aguardar OK
    }
} // Fim da função iniciar_eleicao

// Responde pedidos de eleição vindos de processos menores
static void responder_eleicao(int rank_origem)
{
    int id_origem = rank_origem + 1;                                         // Converte rank em ID legível
    printf("Processo %d recebeu ELEIÇÃO de %d\n", id_processo, id_origem);   // Loga o recebimento
    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD); // Retorna mensagem OK
    if (estado_eleicao == ESTADO_OCIOSO)                                     // Se estava ocioso
    {
        iniciar_eleicao(" (atendeu pedido)", 0); // Inicia eleição própria para desafiar
    }
} // Fim da função responder_eleicao

// Armazena a resposta OK e passa a aguardar o novo coordenador
static void receber_ok(int rank_origem)
{
    printf("Processo %d recebeu OK de %d\n", id_processo, rank_origem + 1); // Loga quem respondeu
    recebeu_ok = 1;                                                         // Marca que algum processo maior continua vivo
    estado_eleicao = ESTADO_ESPERANDO_COORD;                                // Agora aguarda anúncio de coordenador
    inicio_espera_coord = MPI_Wtime();                                      // Salva instante do início dessa espera
} // Fim da função receber_ok

// Atualiza o coordenador local após anúncio
static void receber_coordenador(int novo)
{
    coordenador = novo;                                                       // Atualiza coordenador conhecido
    estado_eleicao = (id_processo == novo) ? ESTADO_LIDER : ESTADO_OCIOSO;    // Ajusta estado local
    printf("Processo %d reconhece %d como coordenador\n", id_processo, novo); // Registra ocorrência
} // Fim da função receber_coordenador

// Varre a fila de mensagens MPI tratando cada tipo de pacote
static void processar_mensagens(void)
{
    while (1) // Loop até não restarem mensagens
    {
        int tem_msg;                                                                // Flag para indicar a presença de mensagens
        MPI_Status status;                                                          // Estrutura que guarda metadados da mensagem
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status); // Verifica sem bloquear
        if (!tem_msg)                                                               // Se não há mensagens
        {
            return; // Sai da função
        }

        int payload = -1;                                                                           // Valor padrão para dados recebidos
        MPI_Recv(&payload, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status); // Captura a mensagem

        if (esta_offline) // Se o processo está offline
        {
            if (status.MPI_TAG == TAG_ELEICAO) // Caso tenha recebido eleição
            {
                printf("Processo %d offline ignorou ELEIÇÃO de %d\n", id_processo, status.MPI_SOURCE + 1); // Log informativo
            }
            continue; // Ignora qualquer processamento
        }

        switch (status.MPI_TAG) // Decide como tratar pela tag
        {
        case TAG_ELEICAO:                         // Mensagem de eleição recebida
            responder_eleicao(status.MPI_SOURCE); // Responde com OK
            break;                                // Sai do switch
        case TAG_OK:                              // Mensagem OK recebida
            receber_ok(status.MPI_SOURCE);        // Atualiza estado com OK
            break;                                // Sai do switch
        case TAG_COORDENADOR:                     // Mensagem de anúncio de coordenador
            receber_coordenador(payload);         // Atualiza líder
            break;                                // Sai do switch
        default:                                  // Qualquer outra tag
            break;                                // Ignora
        }
    }
} // Fim da função processar_mensagens

// Reinicia a eleição se o processo ficou esperando demais
static void verificar_timeouts(void)
{
    if (esta_offline) // Processo offline não verifica timeouts
    {
        // Processo offline só registra quem tentou chamá-lo
        return; // Sai imediatamente
    }

    double agora = MPI_Wtime();                                                                              // Obtém tempo atual
    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok && (agora - inicio_eleicao) >= TEMPO_ESPERA_OK) // Verifica expiração do OK
    {
        printf("Processo %d não recebeu OK; assume liderança\n", id_processo); // Loga motivo
        anunciar_coordenador();                                                // Assume coordenação
    }
    if (estado_eleicao == ESTADO_ESPERANDO_COORD && (agora - inicio_espera_coord) >= TEMPO_ESPERA_COORD) // Verifica anúncio tardio
    {
        printf("Processo %d não recebeu anúncio; reinicia eleição\n", id_processo); // Registra timeout
        estado_eleicao = ESTADO_OCIOSO;                                             // Volta para estado ocioso
        iniciar_eleicao(" (timeout)", 1);                                           // Reinicia eleição forçada
    }
} // Fim da função verificar_timeouts

// Lê parâmetros da linha de comando e monta o cenário solicitado
static void configurar_cenario(int argc, char **argv)
{

    if (argc > 1) // Caso exista argumento para processo falho
    {
        processo_falho = atoi(argv[1]); // Converte string em inteiro
    }
    if (argc > 2) // Caso exista argumento para detector
    {
        processo_detector = atoi(argv[2]); // Armazena detector customizado
    }
    if (argc > 3) // Caso seja informado o tempo de queda
    {
        tempo_queda = atof(argv[3]); // Converte para double
    }
    if (argc > 4) // Caso seja informado o tempo de retorno
    {
        tempo_retorno = atof(argv[4]); // Armazena novo retorno
    }

    if (processo_falho < 1 || processo_falho > total_processos) // Valida ID do processo falho
    {
        processo_falho = -1; // Usa valor inválido para ignorar
    }
    if (processo_detector < 1 || processo_detector > total_processos) // Valida ID do detector
    {
        processo_detector = -1; // Mesma estratégia de fallback
    }
    if (tempo_retorno <= tempo_queda) // Garante cronologia correta
    {
        // Garante que o retorno ocorra em um instante posterior à queda
        tempo_retorno = tempo_queda + 3.0; // Ajusta retorno mínimo
    }

    if (rank_mpi == 0) // Apenas um processo imprime a configuração
    {
        printf("Configuração: falho=%d detector=%d queda=%.1fs retorno=%.1fs\n", // Constrói saída amigável
               processo_falho, processo_detector, tempo_queda, tempo_retorno);   // Exibe parâmetros vigentes
    }
} // Fim da função configurar_cenario

// Programa principal: inicializa MPI e executa a simulação
int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);                          // Inicializa o ambiente MPI
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);        // Obtém rank deste processo
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos); // Obtém total de processos

    id_processo = rank_mpi + 1;     // Converte rank em ID legível
    configurar_cenario(argc, argv); // Carrega parâmetros de execução

    printf("Processo %d inicializado\n", id_processo); // Indica início deste processo
    MPI_Barrier(MPI_COMM_WORLD);                       // Sincroniza todos antes da eleição

    if (id_processo == total_processos) // Verifica se este é o maior ID
    {
        // Maior ID vira coordenador inicial
        anunciar_coordenador(); // Dispara anúncio de coordenação
    }
    else // Demais processos aguardam
    {
        // Os demais aguardam o anúncio inicial
        int msg_coord;                                                                              // Armazena ID do coordenador inicial
        MPI_Status status;                                                                          // Metadados da mensagem recebida
        MPI_Recv(&msg_coord, 1, MPI_INT, MPI_ANY_SOURCE, TAG_COORDENADOR, MPI_COMM_WORLD, &status); // Espera anúncio
        receber_coordenador(msg_coord);                                                             // Atualiza com o coordenador informado
    }

    MPI_Barrier(MPI_COMM_WORLD); // Sincroniza antes da simulação principal

    double inicio = MPI_Wtime(); // Marca início da simulação temporal
    int detector_disparado = 0;  // Impede múltiplas detecções
    const double DURACAO = 12.0; // Duração total da simulação em segundos

    while (MPI_Wtime() - inicio < DURACAO) // Simula até atingir a duração
    {
        processar_mensagens(); // Trata mensagens pendentes
        verificar_timeouts();  // Gerencia timeouts de eleição

        double decorrido = MPI_Wtime() - inicio; // Calcula tempo decorrido

        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda) // Confere se é hora da queda
        {
            // Simula a queda do coordenador "valentão"
            esta_offline = 1;                                    // Marca processo como offline
            ja_caiu = 1;                                         // Evita disparar queda novamente
            estado_eleicao = ESTADO_OCIOSO;                      // Reseta estado de eleição
            printf("\n--- Processo %d caiu ---\n", id_processo); // Loga evento
        }

        if (!detector_disparado && processo_detector == id_processo && decorrido >= tempo_queda + 1.0) // Monitora se detector deve reagir
        {
            // Processo detector percebe o silêncio do coordenador
            detector_disparado = 1;                                                                       // Evita repetição do alerta
            printf("\n=== Processo %d detectou falha do coordenador %d ===\n", id_processo, coordenador); // Log informativo
            iniciar_eleicao(" (detecção)", 0);                                                            // Dispara eleição em resposta
        }

        if (processo_falho == id_processo && esta_offline && !ja_voltou && decorrido >= tempo_retorno) // Verifica condição de retorno
        {
            // Processo que havia caído retorna e força nova eleição
            esta_offline = 0;                                                        // Marca processo como online novamente
            ja_voltou = 1;                                                           // Garante que não volte duas vezes
            printf("\n=== Processo %d voltou e convoca eleição ===\n", id_processo); // Loga retorno
            iniciar_eleicao(" (retorno)", 1);                                        // Força nova eleição ao voltar
        }

        usleep(20000); // Dá pequena pausa para não inundar o terminal
    }

    MPI_Barrier(MPI_COMM_WORLD); // Sincroniza antes de finalizar
    if (rank_mpi == 0)           // Apenas rank 0 imprime cabeçalho final
    {
        printf("\n=== ELEIÇÃO CONCLUÍDA ===\n"); // Mensagem final de resumo
    }
    printf("Processo %d - Coordenador final: %d\n", id_processo, coordenador); // Cada processo relata quem reconhece

    MPI_Finalize(); // Encerra o ambiente MPI
    return 0;       // Finaliza programa principal
} // Fim da função main
