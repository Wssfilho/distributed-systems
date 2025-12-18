#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Tags that identify the types of MPI messages exchanged between processes
#define TAG_ELEICAO 1     // Tag for election start messages
#define TAG_OK 2          // Tag for OK replies (alive process with higher ID)
#define TAG_COORDENADOR 3 // Tag for new coordinator announcements

#define TEMPO_ESPERA_OK 2.0    // Seconds waiting for OK replies from higher-ID processes
#define TEMPO_ESPERA_COORD 4.0 // Seconds waiting for the new coordinator announcement

// Enumeration of the possible states of a process during the election
typedef enum
{
    ESTADO_OCIOSO,          // Process is not participating in an election
    ESTADO_ESPERANDO_OK,    // Process is waiting for OK replies from higher-ID processes
    ESTADO_ESPERANDO_COORD, // Process is waiting to learn who the coordinator is
    ESTADO_LIDER            // Process is the current coordinator
} Estado;

// Global variables storing MPI process information
int rank_mpi;        // Process rank in MPI communicator (0 to N-1)
int total_processos; // Total number of processes in the run
int id_processo;     // Process ID used throughout this code (same as rank)

// Election control variables
int coordenador = -1;                  // Current coordinator ID (-1 = none)
Estado estado_eleicao = ESTADO_OCIOSO; // Current election state of this process

// Simulation scenario parameters (configured via command line)
int processo_detector = -1; // ID of the process that detects the failure and starts the election
int processo_falho = -1;    // ID of the process that will go down (simulate failure)
int processo_volta = 1;     // Flag: 1 = failed process returns, 0 = does not return

// Simulation timing
double tempo_queda = 2.0;   // Time until the process fails
double tempo_retorno = 9.0; // Time until the process returns

// Flags controlling process status and the election
int esta_offline = 0; // Flag: 1 = process is offline, 0 = online
int ja_caiu = 0;      // Flag: 1 = process has already gone down
int ja_voltou = 0;    // Flag: 1 = process has already returned after going down
int recebeu_ok = 0;   // Flag: 1 = received at least one OK in the current election

// Timestamps used to control timeouts
double inicio_eleicao = 0.0;      // Timestamp of the current election start
double inicio_espera_coord = 0.0; // Timestamp of when we started waiting for a coordinator

// Announces this process as the new coordinator to everyone
void anunciar_coordenador(void)
{
    coordenador = id_processo;
    estado_eleicao = ESTADO_LIDER;
    printf("\n>>> Process %d is the new COORDINATOR <<<\n", id_processo);

    // Send coordinator message to all other processes
    for (int i = 0; i < total_processos; i++)
    {
        if (i != rank_mpi)
        {
            MPI_Send(&id_processo, 1, MPI_INT, i, TAG_COORDENADOR, MPI_COMM_WORLD);
        }
    }
}

// Starts an election by sending messages to higher-ID processes
// Parameters: motivo - descriptive string, forcar - ignore "already in election" guard
void iniciar_eleicao(const char *motivo, int forcar)
{
    // Check if we are allowed to start an election
    if (esta_offline || (!forcar && (estado_eleicao == ESTADO_ESPERANDO_OK ||
                                     estado_eleicao == ESTADO_ESPERANDO_COORD)))
    {
        return; // Offline process or already in an election should not start a new one
    }

    // Print election start and the reason
    printf("Process %d starting election%s\n", id_processo, motivo ? motivo : "");
    recebeu_ok = 0;               // Reset OK-received flag
    inicio_eleicao = MPI_Wtime(); // Mark election start timestamp

    int enviou = 0; // Flag to track whether we sent any messages
    // Send ELECTION message to all processes with higher rank
    for (int i = rank_mpi + 1; i < total_processos; i++)
    {
        // Send an election message to process i
        MPI_Send(&id_processo, 1, MPI_INT, i, TAG_ELEICAO, MPI_COMM_WORLD);
        printf("Process %d -> ELECTION -> process %d\n", id_processo, i);
        enviou = 1; // Mark that at least one message was sent
    }

    // If we sent messages, wait for OK replies
    if (enviou)
    {
        estado_eleicao = ESTADO_ESPERANDO_OK; // Switch state to waiting for OK
    }
    else // No higher processes exist
    {
        anunciar_coordenador(); // This process becomes the coordinator
    }
}

// Called when this process receives an ELECTION message
// Parameter: rank_origem - MPI rank of the process that started the election
void responder_eleicao(int rank_origem)
{
    printf("Process %d received ELECTION from %d\n", id_processo, rank_origem);

    // Reply OK to the process that started the election
    MPI_Send(&id_processo, 1, MPI_INT, rank_origem, TAG_OK, MPI_COMM_WORLD);

    // If idle, start our own election (compete for coordinator)
    if (estado_eleicao == ESTADO_OCIOSO)
    {
        iniciar_eleicao(" (answered request)", 0);
    }
}

// Called when this process receives an OK reply
// Parameter: rank_origem - MPI rank of the process that sent OK
void receber_ok(int rank_origem)
{
    printf("Process %d received OK from %d\n", id_processo, rank_origem);
    recebeu_ok = 1; // Mark that we received at least one OK

    // If we were waiting for OK, now wait for the coordinator announcement
    if (estado_eleicao == ESTADO_ESPERANDO_OK)
    {
        estado_eleicao = ESTADO_ESPERANDO_COORD;
        inicio_espera_coord = MPI_Wtime();
    }
}

// Called when this process receives a new coordinator announcement
// Parameter: novo - ID of the process that became coordinator
void receber_coordenador(int novo)
{
    coordenador = novo;
    estado_eleicao = (id_processo == novo) ? ESTADO_LIDER : ESTADO_OCIOSO;
    printf("Process %d acknowledges %d as coordinator\n", id_processo, novo);
}

// Processes all pending MPI messages from the queue
void processar_mensagens(void)
{
    int tem_msg;       // Flag indicating whether a message is available
    MPI_Status status; // Struct with information about the received message
    int carregamento;  // Holds the message payload

    // Loop que processa todas as mensagens disponíveis
    while (MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &tem_msg, &status) == MPI_SUCCESS && tem_msg)
    {
        // Receive the message (blocking)
        MPI_Recv(&carregamento, 1, MPI_INT, status.MPI_SOURCE, status.MPI_TAG,
                 MPI_COMM_WORLD, &status);

        // If the process is offline, only log ignored elections
        if (esta_offline)
        {
            if (status.MPI_TAG == TAG_ELEICAO)
            {
                printf("Process %d (offline) ignored ELECTION from %d\n",
                       id_processo, status.MPI_SOURCE);
            }
            continue; // Skip processing
        }

        // Process the message based on its tag
        switch (status.MPI_TAG)
        {
        case TAG_ELEICAO:                         // Election message received
            responder_eleicao(status.MPI_SOURCE); // Reply with OK
            break;
        case TAG_OK:                       // OK message received
            receber_ok(status.MPI_SOURCE); // Handle OK
            break;
        case TAG_COORDENADOR:                  // Coordinator announcement message
            receber_coordenador(carregamento); // Update coordinator
            break;
        }
    }
}

// Checks whether timeouts occurred
void verificar_timeouts(void)
{
    if (esta_offline) // If process is offline
        return;       // Don't check timeouts

    double agora = MPI_Wtime(); // Obtém timestamp atual

    // OK wait timeout:
    // If we were waiting for OK, received none and time expired
    if (estado_eleicao == ESTADO_ESPERANDO_OK && !recebeu_ok &&
        (agora - inicio_eleicao) >= TEMPO_ESPERA_OK)
    {
        // No higher process replied, so we become coordinator
        printf("Process %d did not receive OK; becoming leader\n", id_processo);
        anunciar_coordenador(); // Announce ourselves as coordinator
        return;                 // Skip checking the next timeout
    }

    // Coordinator announcement wait timeout:
    // If we were waiting for a coordinator and time expired
    if (estado_eleicao == ESTADO_ESPERANDO_COORD &&
        (agora - inicio_espera_coord) >= TEMPO_ESPERA_COORD)
    {
        // Coordinator did not announce, restart election
        printf("Process %d did not receive announcement; restarting election\n", id_processo);
        estado_eleicao = ESTADO_OCIOSO;   // Back to idle
        iniciar_eleicao(" (timeout)", 1); // Forced election
    }
}

// Parses command line arguments and configures the scenario
// Parameters: argc - argument count, argv - argument vector
void configurar_cenario(int argc, char **argv)
{
    // Check whether enough arguments were provided
    if (argc < 3)
    {
        if (rank_mpi == 0) // Only rank 0 prints usage
        {
            // Print program usage
            printf("Usage: mpirun -np <N> %s <offline_process> <detector_process> [process_returns]\n", argv[0]);
            printf("  offline_process: ID of the process that will go down (0 to N-1)\n");
            printf("  detector_process: ID of the process that detects the failure and starts the election (0 to N-1)\n");
            printf("  process_returns: (optional) 1=returns, 0=does not return (default: 1)\n");
        }
        MPI_Finalize(); // Finalize MPI environment
        exit(1);        // Exit with error
    }

    // Read required arguments
    processo_falho = atoi(argv[1]);    // Process that will go down
    processo_detector = atoi(argv[2]); // Failure detector process

    // Read optional argument (whether the failed process returns)
    if (argc > 3)
        processo_volta = (atoi(argv[3]) == 1); // If argv[3]==1 it returns; otherwise it doesn't

    // Validate parameters
    if (processo_falho < 0 || processo_falho >= total_processos ||       // Failed process out of range
        processo_detector < 0 || processo_detector >= total_processos || // Detector out of range
        processo_detector == processo_falho)                             // Detector cannot be the one that goes down
    {
        if (rank_mpi == 0) // Only rank 0 prints error
        {
            printf("Error: invalid parameters\n");
        }
        MPI_Finalize(); // Finalize MPI environment
        exit(1);        // Exit with error
    }

    // Print scenario configuration (rank 0 only)
    if (rank_mpi == 0)
    {
        printf("=== Configuration ===\n");
        // Print main parameters in one line
        printf("Processes: %d | Offline: %d | Detector: %d | Returns: %s\n",
               total_processos, processo_falho, processo_detector,
               processo_volta ? "YES" : "NO");
        // Print simulation timing
        printf("Times: failure=%.1fs%s\n", tempo_queda,
               processo_volta ? " | return=9.0s" : "");
        printf("====================\n\n");
    }
}

// Função principal do programa
int main(int argc, char **argv)
{
    // Initialize the MPI environment
    MPI_Init(&argc, &argv);
    // Get this process rank
    MPI_Comm_rank(MPI_COMM_WORLD, &rank_mpi);
    // Get the total number of processes
    MPI_Comm_size(MPI_COMM_WORLD, &total_processos);

    // Process ID is the rank itself (0 to N-1)
    id_processo = rank_mpi;
    // Configure scenario based on CLI arguments
    configurar_cenario(argc, argv);

    // Print initialization message
    printf("Process %d initialized\n", id_processo);
    // Synchronize all processes before starting the simulation
    MPI_Barrier(MPI_COMM_WORLD);

    // Mark simulation start timestamp
    double inicio = MPI_Wtime();
    // Flag to ensure the detector triggers only once
    int detector_disparado = 0;

    // Main simulation loop (12 seconds)
    while (MPI_Wtime() - inicio < 12.0)
    {
        // Process all pending MPI messages
        processar_mensagens();
        // Check timeouts and act accordingly
        verificar_timeouts();

        // Compute elapsed time since simulation start
        double decorrido = MPI_Wtime() - inicio;

        // Simulate failure of the selected process at the configured time
        if (!ja_caiu && processo_falho == id_processo && decorrido >= tempo_queda)
        {
            esta_offline = 1;               // Mark process as offline
            ja_caiu = 1;                    // Prevent failing twice
            estado_eleicao = ESTADO_OCIOSO; // Reset election state
            printf("\n--- Process %d crashed (offline) ---\n", id_processo);
        }

        // Detector notices the failure and starts an election (0.5s after failure)
        if (!detector_disparado && processo_detector == id_processo &&
            decorrido >= tempo_queda + 0.5)
        {
            detector_disparado = 1; // Prevent multiple triggers
            // Print detection message
            printf("\n=== Process %d detected that coordinator process %d is not responding ===\n",
                   id_processo, total_processos - 1);
            // Start forced election
            iniciar_eleicao(" (detected failure)", 1);
        }

        // Simulate process return (if configured to return)
        if (processo_volta && processo_falho == id_processo && esta_offline &&
            !ja_voltou && decorrido >= tempo_retorno)
        {
            esta_offline = 0; // Mark process as online again
            ja_voltou = 1;    // Prevent returning multiple times
            printf("\n=== Process %d is back and calls an election ===\n", id_processo);
            // Start forced election after returning
            iniciar_eleicao(" (return)", 1);
        }

        // Sleep 20ms to avoid overloading CPU/terminal
        usleep(20000);
    }

    // Synchronize all processes before finishing
    MPI_Barrier(MPI_COMM_WORLD);
    // Only rank 0 prints the final header
    if (rank_mpi == 0)
        printf("\n=== ELECTION COMPLETED ===\n");
    // Each process prints its final coordinator
    printf("Process %d - Final coordinator: %d\n", id_processo, coordenador);

    // Finalize the MPI environment
    MPI_Finalize();
    return 0; // Success
}
