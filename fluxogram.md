# 🔄 Bully Algorithm Flowchart

## 📋 Main Flow Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        PROGRAM START                              │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   MPI_Init()            │
                    │   - Initializes MPI     │
                    │   - Gets rank           │
                    │   - Gets total procs    │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ configurar_cenario()    │
                    │ - Reads CLI arguments   │
                    │ - Validates parameters  │
                    │ - Prints configuration  │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ All processes start     │
                    │ with NO coordinator     │
                    │ coordenador = -1        │
                    │ estado = OCIOSO         │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   MPI_Barrier()         │
                    │   (Synchronization)     │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   MAIN LOOP             │
                    │   (12 segundos)         │
                    └─────────────────────────┘
```

## 🔁 Main Loop (Detailed)

```
┌──────────────────────────────────────────────────────────────────┐
│                         MAIN LOOP                                  │
│                    (while tempo < 12.0s)                         │
└────┬─────────────────────────────────────────────────────────┬───┘
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
    ├─►│ 1. processar_mensagens()                       │     │
    │  │    - Checks MPI queue                          │     │
    │  │    - Handles TAG_ELEICAO, TAG_OK, TAG_COORD    │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
    ├─►│ 2. verificar_timeouts()                        │     │
    │  │    - OK timeout (2s) → becomes coordinator     │     │
    │  │    - COORD timeout (4s) → restarts election    │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
    ├─►│ 3. FAILURE SIMULATION (time = 2.0s)            │     │
     │  │    Se (processo_falho == meu_id)               │     │
     │  │    └─► esta_offline = 1                        │     │
    │  │        printf("crashed")                        │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
    ├─►│ 4. DETECTION (time = 2.5s)                     │     │
     │  │    Se (processo_detector == meu_id)            │     │
    │  │    └─► printf("detected failure")              │     │
    │  │        iniciar_eleicao("detected failure")     │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
    ├─►│ 5. RETURN (time = 9.0s, IF processo_volta=1)   │     │
     │  │    Se (processo_falho == meu_id && voltou)     │     │
     │  │    └─► esta_offline = 0                        │     │
    │  │        printf("back")                           │     │
    │  │        iniciar_eleicao("return")               │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
    └─►│ 6. usleep(20ms) - pause to avoid overload      │     │
        └────────────────────────────────────────────────┘     │
                                                                │
                              Loop continua ◄──────────────────┘
```

## 🗳️ Function Flow: iniciar_eleicao()

```
                    ┌──────────────────────┐
                    │  iniciar_eleicao()   │
                    └──────────┬───────────┘
                               │
                ┌──────────────▼──────────────┐
                │ Can I start an election?    │
                │ - NOT offline?              │
                │ - NOT already in election?  │
                │   (ou está forçando?)       │
                └──────────┬──────────────────┘
                           │
                    ┌──────▼──────┐
                    │  No   │ Yes │
                    └───┬───┴──┬──┘
                        │      │
                    return     │
                               │
                    ┌──────────▼──────────────┐
                    │ printf("starting...")    │
                    │ recebeu_ok = 0           │
                    │ inicio_eleicao = agora   │
                    └──────────┬───────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ FOR (i = meu_rank+1     │
                    │      até total_proc)    │
                    │ ┌────────────────────┐  │
                    │ │ MPI_Send()         │  │
                    │ │ TAG_ELEICAO        │  │
                    │ │ para processo i    │  │
                    │ └────────────────────┘  │
                    └──────────┬───────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Sent any message?       │
                    └──────┬────────────┬─────┘
                           │            │
                       Não │            │ Sim
                           │            │
                ┌──────────▼─────┐  ┌──▼───────────────────┐
                │ I'M THE HIGHEST!│  │ state = ESPERANDO_OK│
                │ anunciar_      │  └──────────────────────┘
                │ coordenador()  │
                └────────────────┘
```

## 💬 Function Flow: processar_mensagens()

```
                    ┌──────────────────────┐
                    │ processar_mensagens()│
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ while (true)            │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ MPI_Iprobe()            │
                    │ (checks message without │
                    │  blocking)              │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Message available?      │
                    └──────┬────────────┬─────┘
                           │            │
                       No  │            │ Yes
                           │            │
                      ┌────▼────┐  ┌────▼──────────────┐
                      │ return  │  │ MPI_Recv()        │
                      └─────────┘  │ (recebe msg)      │
                                   └────┬──────────────┘
                                        │
                                   ┌────▼──────────────┐
                                   │ Am I offline?     │
                                   └────┬──────────┬───┘
                                        │          │
                                    Yes │          │ No
                                        │          │
                              ┌─────────▼───┐  ┌──▼────────────────┐
                              │ printf      │  │ switch(status.TAG)│
                              │ "ignored"   │  └──┬────────────────┘
                              │ continue    │     │
                              └─────────────┘     │
                                                  │
                    ┌─────────────────────────────┼─────────────────────────┐
                    │                             │                         │
         ┌──────────▼──────────┐   ┌─────────────▼───────────┐  ┌─────────▼──────────┐
         │ TAG_ELEICAO         │   │ TAG_OK                  │  │ TAG_COORDENADOR    │
         │ responder_eleicao() │   │ receber_ok()            │  │ receber_coordenador│
         └─────────────────────┘   └─────────────────────────┘  └────────────────────┘
                    │                             │                         │
                    │                             │                         │
                    └─────────────────────────────┴─────────────────────────┘
                                                  │
                                            (back to loop)
```

## 📨 Flow: responder_eleicao()

```
                    ┌──────────────────────┐
                    │ responder_eleicao()  │
                    │ (someone sent me an  │
                    │  ELECTION message)   │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ printf("received        │
                    │ ELECTION from X")       │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ MPI_Send()              │
                    │ TAG_OK para X           │
                    │ (I reply I'm alive and  │
                    │  have a higher ID)      │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Am I IDLE?              │
                    └──────┬────────────┬─────┘
                           │            │
                       No  │            │ Yes
                           │            │
                      ┌────▼────┐  ┌────▼──────────────┐
                      │ return  │  │ iniciar_eleicao() │
                      └─────────┘  │ (I also compete!) │
                                   └───────────────────┘
```

## ✅ Flow: receber_ok()

```
                    ┌──────────────────────┐
                    │   receber_ok()       │
                    │ (received reply from │
                    │  higher process)     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ printf("received OK")   │
                    │ recebeu_ok = 1          │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Was I ESPERANDO_OK?     │
                    └──────┬────────────┬─────┘
                           │            │
                       No  │            │ Yes
                           │            │
                      ┌────▼────┐  ┌────▼──────────────────┐
                      │ return  │  │ estado =              │
                      └─────────┘  │ ESPERANDO_COORD       │
                                   │ inicio_espera = agora │
                                   └───────────────────────┘
```

## 👑 Flow: anunciar_coordenador()

```
                    ┌──────────────────────┐
                    │ anunciar_coordenador│
                    │ (I'M THE LEADER!)    │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ coordenador = meu_id    │
                    │ estado = LIDER          │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ printf(">>> I'M THE NEW │
                    │ COORDINATOR <<<")       │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ FOR (all processes)     │
                    │ ┌────────────────────┐  │
                    │ │ MPI_Send()         │  │
                    │ │ TAG_COORDENADOR    │  │
                    │ │ (announce myself)  │  │
                    │ └────────────────────┘  │
                    └─────────────────────────┘
```

## ⏱️ Flow: verificar_timeouts()

```
                    ┌──────────────────────┐
                    │ verificar_timeouts() │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Am I offline?           │
                    └──────┬────────────┬─────┘
                           │            │
                       Yes │            │ No
                           │            │
                      ┌────▼────┐       │
                      │ return  │       │
                      └─────────┘       │
                                        │
                    ┌───────────────────▼───────────────────┐
                    │ TIMEOUT 1: Waiting for OK (2s)        │
                    │ Se (estado==ESPERANDO_OK &&           │
                    │     !recebeu_ok &&                    │
                    │     tempo >= 2s)                      │
                    │ └─► printf("didn't receive OK")       │
                    │     anunciar_coordenador()            │
                    └───────────────────┬───────────────────┘
                                        │
                    ┌───────────────────▼───────────────────┐
                    │ TIMEOUT 2: Waiting for COORD (4s)     │
                    │ Se (estado==ESPERANDO_COORD &&        │
                    │     tempo >= 4s)                      │
                    │ └─► printf("didn't receive announcement")│
                    │     estado = OCIOSO                   │
                    │     iniciar_eleicao("timeout")        │
                    └───────────────────────────────────────┘
```

## 🎯 Process States

```
┌──────────────────────────────────────────────────────────────┐
│                     STATE MACHINE                             │
└──────────────────────────────────────────────────────────────┘

    IDLE ──────────────────────────────────────────┐
       │                                            │
    │ iniciar_eleicao()                         │
       │                                            │
       ▼                                            │
    WAITING_OK ──────────┐                           │
       │                │                           │
    │ got OK         │ timeout (2s)              │
    │                │ no OK                     │
       ▼                ▼                           │
    WAITING_COORD    LEADER ◄────────────────────────┤
       │                │                           │
    │ timeout (4s)   │ receives COORDINATOR     │
    │                │ from higher ID           │
       └────────────────┴───────────────────────────┘
                         │
                         ▼
                      IDLE
```

## 📊 Timeline (Example: 5 processes, P3 crashes, P1 detects)

```
Time  │ P1              │ P2       │ P3           │ P4       │ P5
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
0.0s  │ IDLE            │ IDLE     │ IDLE         │ IDLE     │ IDLE
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.0s  │ IDLE            │ IDLE     │ ⚠️ OFFLINE   │ IDLE     │ IDLE
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.5s  │ 🔍 Detects P3   │ IDLE     │ OFFLINE      │ IDLE     │ IDLE
    │ ELECTION→P2-P5  │          │              │          │
    │ WAITING_OK      │          │              │          │
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.5s+ │ WAITING_OK      │ OK→P1    │ OFFLINE      │ OK→P1    │ OK→P1
    │                 │ ELECTION │              │ ELECTION │ ELECTION
    │                 │ →P3-P5   │              │ →P5      │
    │                 │ WAITING_OK│             │ WAITING_OK│ WAITING_OK
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.6s  │ got OK(P2)      │ OK(P4)   │ OFFLINE      │ OK(P5)   │ OK→P2
    │ got OK(P4)      │ OK(P5)   │              │ WAITING_COORD│ OK→P4
    │ got OK(P5)      │ WAITING_COORD│          │          │ 👑 LEADER
    │ WAITING_COORD   │          │              │          │
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.6s+ │ COORD=P5        │ COORD=P5 │ OFFLINE      │ COORD=P5 │ ANNOUNCES
    │ IDLE            │ IDLE     │              │ IDLE     │ LEADER
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
9.0s  │ IDLE            │ IDLE     │ 🔄 BACK      │ IDLE     │ LEADER
    │                 │          │ ELECTION→P5  │          │
    │                 │          │ WAITING_OK   │          │
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
9.0s+ │ IDLE            │ IDLE     │ OK(P5)       │ IDLE     │ OK→P3
    │                 │          │ WAITING_COORD│          │ 👑 LEADER
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
9.1s  │ COORD=P5        │ COORD=P5 │ COORD=P5     │ COORD=P5 │ ANNOUNCES
    │ IDLE            │ IDLE     │ IDLE         │ IDLE     │ LEADER
```

## 🔑 Symbol Legend

```
📨 Sent message
📬 Received message
⚠️  Process offline
🔄 Process returned
🔍 Failure detected
👑 Became coordinator
⏱️  Timeout
✅ OK received
```

## 💡 Summary of Critical Flows

### 1️⃣ **Election Start**
```
Trigger → Sends ELECTION to higher IDs → Waits for OK (2s)
   ↓                                           ↓
    └── No higher IDs → Becomes Coordinator    Got OK? → Waits for Coordinator (4s)
```

### 2️⃣ **Receiving an Election**
```
ELECTION msg → Sends OK → Was idle? → YES → Starts own election
                              ↓
                             NO → Keeps waiting
```

### 3️⃣ **Timeouts**
```
OK timeout (2s) → No OK? → Becomes Coordinator
COORD timeout (4s) → No announcement? → Restarts election
```