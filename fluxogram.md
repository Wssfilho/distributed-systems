# 🔄 Fluxograma do Algoritmo Bully

## 📋 Visão Geral do Fluxo Principal

```
┌─────────────────────────────────────────────────────────────────┐
│                         INÍCIO DO PROGRAMA                       │
└────────────────────────────────┬────────────────────────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   MPI_Init()            │
                    │   - Inicializa MPI      │
                    │   - Obtém rank          │
                    │   - Obtém total proc.   │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ configurar_cenario()    │
                    │ - Lê argumentos CLI     │
                    │ - Valida parâmetros     │
                    │ - Exibe configuração    │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │ Todos os processos      │
                    │ iniciam SEM coordenador │
                    │ coordenador = -1        │
                    │ estado = OCIOSO         │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   MPI_Barrier()         │
                    │   (Sincronização)       │
                    └────────────┬────────────┘
                                 │
                    ┌────────────▼────────────┐
                    │   LOOP PRINCIPAL        │
                    │   (12 segundos)         │
                    └─────────────────────────┘
```

## 🔁 Loop Principal (Detalhado)

```
┌──────────────────────────────────────────────────────────────────┐
│                         LOOP PRINCIPAL                            │
│                    (while tempo < 12.0s)                         │
└────┬─────────────────────────────────────────────────────────┬───┘
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
     ├─►│ 1. processar_mensagens()                       │     │
     │  │    - Verifica fila MPI                         │     │
     │  │    - Processa TAG_ELEICAO, TAG_OK, TAG_COORD   │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
     ├─►│ 2. verificar_timeouts()                        │     │
     │  │    - Timeout OK (2s) → vira coordenador        │     │
     │  │    - Timeout COORD (4s) → reinicia eleição     │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
     ├─►│ 3. SIMULAÇÃO DE QUEDA (tempo = 2.0s)           │     │
     │  │    Se (processo_falho == meu_id)               │     │
     │  │    └─► esta_offline = 1                        │     │
     │  │        printf("caiu")                           │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
     ├─►│ 4. DETECÇÃO (tempo = 2.5s)                     │     │
     │  │    Se (processo_detector == meu_id)            │     │
     │  │    └─► printf("detectou queda")                │     │
     │  │        iniciar_eleicao("detectou queda")       │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
     ├─►│ 5. RETORNO (tempo = 9.0s, SE processo_volta=1) │     │
     │  │    Se (processo_falho == meu_id && voltou)     │     │
     │  │    └─► esta_offline = 0                        │     │
     │  │        printf("voltou")                         │     │
     │  │        iniciar_eleicao("retorno")              │     │
     │  └────────────────────────────────────────────────┘     │
     │                                                           │
     │  ┌────────────────────────────────────────────────┐     │
     └─►│ 6. usleep(20ms) - pausa para não travar        │     │
        └────────────────────────────────────────────────┘     │
                                                                │
                              Loop continua ◄──────────────────┘
```

## 🗳️ Fluxo da Função: iniciar_eleicao()

```
                    ┌──────────────────────┐
                    │  iniciar_eleicao()   │
                    └──────────┬───────────┘
                               │
                ┌──────────────▼──────────────┐
                │ Posso iniciar eleição?      │
                │ - NÃO está offline?         │
                │ - NÃO está em eleição já?   │
                │   (ou está forçando?)       │
                └──────────┬──────────────────┘
                           │
                    ┌──────▼──────┐
                    │  Não  │ Sim │
                    └───┬───┴──┬──┘
                        │      │
                    return     │
                               │
                    ┌──────────▼──────────────┐
                    │ printf("iniciando...")   │
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
                    │ Enviou alguma msg?      │
                    └──────┬────────────┬─────┘
                           │            │
                       Não │            │ Sim
                           │            │
                ┌──────────▼─────┐  ┌──▼───────────────────┐
                │ SOU O MAIOR!   │  │ estado = ESPERANDO_OK│
                │ anunciar_      │  └──────────────────────┘
                │ coordenador()  │
                └────────────────┘
```

## 💬 Fluxo da Função: processar_mensagens()

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
                    │ (verifica msg sem       │
                    │  bloquear)              │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Tem mensagem?           │
                    └──────┬────────────┬─────┘
                           │            │
                       Não │            │ Sim
                           │            │
                      ┌────▼────┐  ┌────▼──────────────┐
                      │ return  │  │ MPI_Recv()        │
                      └─────────┘  │ (recebe msg)      │
                                   └────┬──────────────┘
                                        │
                                   ┌────▼──────────────┐
                                   │ Estou offline?    │
                                   └────┬──────────┬───┘
                                        │          │
                                    Sim │          │ Não
                                        │          │
                              ┌─────────▼───┐  ┌──▼────────────────┐
                              │ printf      │  │ switch(status.TAG)│
                              │ "ignorou"   │  └──┬────────────────┘
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
                                            (volta ao loop)
```

## 📨 Fluxo: responder_eleicao()

```
                    ┌──────────────────────┐
                    │ responder_eleicao()  │
                    │ (alguém me mandou    │
                    │  msg de ELEIÇÃO)     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ printf("recebeu         │
                    │ ELEIÇÃO de X")          │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ MPI_Send()              │
                    │ TAG_OK para X           │
                    │ (respondo que estou     │
                    │  vivo e sou maior)      │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Estou OCIOSO?           │
                    └──────┬────────────┬─────┘
                           │            │
                       Não │            │ Sim
                           │            │
                      ┌────▼────┐  ┌────▼──────────────┐
                      │ return  │  │ iniciar_eleicao() │
                      └─────────┘  │ (me candidato tb!)│
                                   └───────────────────┘
```

## ✅ Fluxo: receber_ok()

```
                    ┌──────────────────────┐
                    │   receber_ok()       │
                    │ (recebi resposta de  │
                    │  processo maior)     │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ printf("recebeu OK")    │
                    │ recebeu_ok = 1          │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Estava ESPERANDO_OK?    │
                    └──────┬────────────┬─────┘
                           │            │
                       Não │            │ Sim
                           │            │
                      ┌────▼────┐  ┌────▼──────────────────┐
                      │ return  │  │ estado =              │
                      └─────────┘  │ ESPERANDO_COORD       │
                                   │ inicio_espera = agora │
                                   └───────────────────────┘
```

## 👑 Fluxo: anunciar_coordenador()

```
                    ┌──────────────────────┐
                    │ anunciar_coordenador│
                    │ (SOU O LÍDER!)       │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ coordenador = meu_id    │
                    │ estado = LIDER          │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ printf(">>> SOU O NOVO  │
                    │ COORDENADOR <<<")       │
                    └──────────┬──────────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ FOR (todos processos)   │
                    │ ┌────────────────────┐  │
                    │ │ MPI_Send()         │  │
                    │ │ TAG_COORDENADOR    │  │
                    │ │ (anuncio-me)       │  │
                    │ └────────────────────┘  │
                    └─────────────────────────┘
```

## ⏱️ Fluxo: verificar_timeouts()

```
                    ┌──────────────────────┐
                    │ verificar_timeouts() │
                    └──────────┬───────────┘
                               │
                    ┌──────────▼──────────────┐
                    │ Estou offline?          │
                    └──────┬────────────┬─────┘
                           │            │
                       Sim │            │ Não
                           │            │
                      ┌────▼────┐       │
                      │ return  │       │
                      └─────────┘       │
                                        │
                    ┌───────────────────▼───────────────────┐
                    │ TIMEOUT 1: Esperando OK (2s)          │
                    │ Se (estado==ESPERANDO_OK &&           │
                    │     !recebeu_ok &&                    │
                    │     tempo >= 2s)                      │
                    │ └─► printf("não recebeu OK")          │
                    │     anunciar_coordenador()            │
                    └───────────────────┬───────────────────┘
                                        │
                    ┌───────────────────▼───────────────────┐
                    │ TIMEOUT 2: Esperando COORD (4s)       │
                    │ Se (estado==ESPERANDO_COORD &&        │
                    │     tempo >= 4s)                      │
                    │ └─► printf("não recebeu anúncio")     │
                    │     estado = OCIOSO                   │
                    │     iniciar_eleicao("timeout")        │
                    └───────────────────────────────────────┘
```

## 🎯 Estados do Processo

```
┌──────────────────────────────────────────────────────────────┐
│                    MÁQUINA DE ESTADOS                         │
└──────────────────────────────────────────────────────────────┘

    OCIOSO ────────────────────────────────────────┐
       │                                            │
       │ iniciar_eleicao()                         │
       │                                            │
       ▼                                            │
  ESPERANDO_OK ────────┐                           │
       │                │                           │
       │ recebeu OK     │ timeout (2s)              │
       │                │ sem OK                    │
       ▼                ▼                           │
  ESPERANDO_COORD   LIDER ◄─────────────────────── ┤
       │                │                           │
       │ timeout (4s)   │ recebe COORDENADOR       │
       │                │ de ID maior              │
       └────────────────┴───────────────────────────┘
                         │
                         ▼
                      OCIOSO
```

## 📊 Linha do Tempo (Exemplo: 5 processos, P3 cai, P1 detecta)

```
Tempo │ P1              │ P2       │ P3           │ P4       │ P5
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
0.0s  │ OCIOSO          │ OCIOSO   │ OCIOSO       │ OCIOSO   │ OCIOSO
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.0s  │ OCIOSO          │ OCIOSO   │ ⚠️ OFFLINE   │ OCIOSO   │ OCIOSO
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.5s  │ 🔍 Detecta P3   │ OCIOSO   │ OFFLINE      │ OCIOSO   │ OCIOSO
      │ ELEIÇÃO→P2-P5   │          │              │          │
      │ ESP_OK          │          │              │          │
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.5s+ │ ESP_OK          │ OK→P1    │ OFFLINE      │ OK→P1    │ OK→P1
      │                 │ ELEIÇÃO  │              │ ELEIÇÃO  │ ELEIÇÃO
      │                 │ →P3-P5   │              │ →P5      │
      │                 │ ESP_OK   │              │ ESP_OK   │ ESP_OK
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.6s  │ recebe OK(P2)   │ OK(P4)   │ OFFLINE      │ OK(P5)   │ OK→P2
      │ recebe OK(P4)   │ OK(P5)   │              │ ESP_COORD│ OK→P4
      │ recebe OK(P5)   │ ESP_COORD│              │          │ 👑 LIDER
      │ ESP_COORD       │          │              │          │
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
2.6s+ │ COORD=P5        │ COORD=P5 │ OFFLINE      │ COORD=P5 │ ANUNCIA
      │ OCIOSO          │ OCIOSO   │              │ OCIOSO   │ LIDER
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
9.0s  │ OCIOSO          │ OCIOSO   │ 🔄 VOLTOU    │ OCIOSO   │ LIDER
      │                 │          │ ELEIÇÃO→P5   │          │
      │                 │          │ ESP_OK       │          │
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
9.0s+ │ OCIOSO          │ OCIOSO   │ OK(P5)       │ OCIOSO   │ OK→P3
      │                 │          │ ESP_COORD    │          │ 👑 LIDER
──────┼─────────────────┼──────────┼──────────────┼──────────┼─────────
9.1s  │ COORD=P5        │ COORD=P5 │ COORD=P5     │ COORD=P5 │ ANUNCIA
      │ OCIOSO          │ OCIOSO   │ OCIOSO       │ OCIOSO   │ LIDER
```

## 🔑 Legenda de Símbolos

```
📨 Mensagem enviada
📬 Mensagem recebida
⚠️  Processo offline
🔄 Processo voltou
🔍 Detectou falha
👑 Virou coordenador
⏱️  Timeout
✅ OK recebido
```

## 💡 Resumo dos Fluxos Críticos

### 1️⃣ **Início de Eleição**
```
Gatilho → Envia ELEIÇÃO para maiores → Aguarda OK (2s)
   ↓                                           ↓
   └── Sem maiores → Vira Coordenador    Recebeu OK? → Aguarda Coordenador (4s)
```

### 2️⃣ **Recebimento de Eleição**
```
Msg ELEIÇÃO → Envia OK → Estava ocioso? → SIM → Inicia própria eleição
                              ↓
                             NÃO → Continua esperando
```

### 3️⃣ **Timeouts**
```
Timeout OK (2s) → Sem OK? → Vira Coordenador
Timeout COORD (4s) → Sem anúncio? → Reinicia eleição
```