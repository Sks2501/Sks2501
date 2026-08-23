# Laboratórios Multilinguagem

Coleção pública de projetos independentes para demonstrar engenharia de sistemas sem depender de empresas, infraestrutura privada, credenciais ou dispositivos reais.

## Estrutura

```text
labs/
├── rust/
│   └── resilient_pipeline/
│       ├── Cargo.toml
│       └── src/main.rs
├── c17/
│   └── bounded_frame_parser/
│       └── main.c
├── cpp20/
│   └── spsc_ring_buffer/
│       └── main.cpp
├── go/
│   └── resilient_worker_pool/
│       ├── go.mod
│       └── main.go
└── typescript/
    └── cqrs_event_store/
        ├── package.json
        ├── tsconfig.json
        └── src/index.ts
```

## Rust — pipeline resiliente

Conceitos demonstrados:

- fila limitada com backpressure;
- múltiplos workers;
- compartilhamento seguro entre threads;
- métricas usando atomics;
- retries limitados;
- backoff exponencial;
- jitter determinístico;
- cancelamento cooperativo;
- isolamento entre falhas permanentes e transitórias.

Executar:

```bash
cargo run --release --manifest-path labs/rust/resilient_pipeline/Cargo.toml
```

## C17 — parser binário limitado

Conceitos demonstrados:

- zero alocação dinâmica no parsing;
- limites de payload explícitos;
- framing binário;
- endianness definida;
- CRC32 IEEE;
- rejeição de frames truncados;
- rejeição de versão desconhecida;
- validação antes de cópia;
- testes negativos embutidos.

Compilar e executar:

```bash
gcc -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror \
  labs/c17/bounded_frame_parser/main.c \
  -o /tmp/bounded_frame_parser
/tmp/bounded_frame_parser
```

## C++20 — SPSC ring buffer

Conceitos demonstrados:

- fila single-producer/single-consumer;
- caminho quente sem mutex;
- índices atômicos;
- semântica acquire/release;
- alinhamento para reduzir false sharing;
- validação de sequência;
- teste com um milhão de eventos;
- medição simples de throughput.

Compilar e executar:

```bash
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Werror -pthread \
  labs/cpp20/spsc_ring_buffer/main.cpp \
  -o /tmp/spsc_ring_buffer
/tmp/spsc_ring_buffer
```

## Go — worker pool resiliente

Conceitos demonstrados:

- worker pool limitado;
- `context.Context` para deadline e cancelamento;
- circuit breaker com closed/open/half-open;
- retry exponencial com jitter;
- backpressure por channels limitados;
- métricas atômicas;
- desligamento limpo;
- distinção entre falha de disponibilidade e rejeição permanente.

Executar:

```bash
cd labs/go/resilient_worker_pool
go run .
```

## TypeScript — CQRS e Event Sourcing

Conceitos demonstrados:

- TypeScript em modo `strict`;
- comandos separados de eventos;
- agregado reconstruído por replay;
- event store append-only em memória;
- optimistic concurrency;
- versionamento de stream;
- idempotência por `commandId`;
- correlação por `correlationId`;
- regras de domínio;
- detecção explícita de conflito concorrente.

Executar:

```bash
cd labs/typescript/cqrs_event_store
npm install
npm run check
npm run build
npm start
```

## Critérios de qualidade

Todo laboratório deve obedecer aos seguintes invariantes:

1. nenhum segredo no código;
2. nenhum endpoint real;
3. nenhum nome de empresa necessário para execução;
4. entradas externas validadas antes do uso;
5. buffers e retries limitados;
6. falhas explicitamente representadas;
7. build reproduzível;
8. código compilável sem etapas ocultas;
9. ausência de placeholders ou trechos incompletos;
10. demonstração autocontida e segura.
