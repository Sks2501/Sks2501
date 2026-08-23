<div align="center">

# Laboratório de Engenharia de Sistemas

### Sistemas Embarcados · Sistemas Distribuídos · Protocolos · Concorrência · Backend

![Rust](https://img.shields.io/badge/Rust-sistemas-111827?style=for-the-badge&logo=rust&logoColor=white)
![C](https://img.shields.io/badge/C17-embedded-111827?style=for-the-badge&logo=c&logoColor=A8B9CC)
![C++](https://img.shields.io/badge/C%2B%2B20-concorrência-111827?style=for-the-badge&logo=cplusplus&logoColor=00599C)
![Go](https://img.shields.io/badge/Go-resiliência-111827?style=for-the-badge&logo=go&logoColor=00ADD8)
![TypeScript](https://img.shields.io/badge/TypeScript-CQRS-111827?style=for-the-badge&logo=typescript&logoColor=3178C6)

</div>

---

## Escopo técnico

Este perfil é intencionalmente técnico e não expõe empresas, marcas, localização, infraestrutura privada ou informações pessoais desnecessárias.

Áreas principais:

- engenharia de software de sistemas;
- concorrência e paralelismo;
- protocolos binários e parsing determinístico;
- sistemas distribuídos e tolerância a falhas;
- CQRS, event sourcing e idempotência;
- controle de backpressure;
- circuit breaker e retry com jitter;
- observabilidade e correlação de requisições;
- estruturas lock-free quando aplicáveis;
- segurança por padrão e validação estrita;
- memória limitada e execução determinística;
- testes de contratos e compatibilidade.

---

## Projetos públicos de engenharia

Os exemplos deste repositório usam apenas dados sintéticos e abstrações genéricas. Nenhum código público depende de credenciais reais, topologia privada, dispositivos reais ou sistemas operacionais de empresas.

```text
labs/
├── rust/
│   └── resilient_pipeline/
├── c17/
│   └── bounded_frame_parser/
├── cpp20/
│   └── spsc_ring_buffer/
├── go/
│   └── resilient_worker_pool/
└── typescript/
    └── cqrs_event_store/
```

### Rust

Pipeline concorrente com fila limitada, múltiplos workers, cancelamento cooperativo, métricas atômicas e isolamento de falhas.

### C17

Parser binário com limites explícitos, framing determinístico, CRC32, rejeição de entrada truncada e nenhuma alocação dinâmica durante o parse.

### C++20

Ring buffer SPSC com índices atômicos, ordenação de memória explícita e sem mutex no caminho quente.

### Go

Worker pool resiliente com `context.Context`, circuit breaker, retry exponencial com jitter, limites de concorrência e desligamento limpo.

### TypeScript

Event store em memória com optimistic concurrency, idempotência, agregados, comandos, eventos de domínio e reconstrução de estado.

---

## Princípios de engenharia

```text
contratos explícitos
    > comportamento implícito

falhas limitadas
    > falhas em cascata

versionamento
    > acoplamento silencioso

observabilidade
    > estado invisível

build reproduzível
    > dependência de ambiente

validação estrita
    > confiança em entrada externa

rollback definido
    > mudanças irreversíveis
```

---

## Arquitetura de referência

```text
┌──────────────────────────────────────────────────────────────┐
│                      APLICAÇÃO / API                         │
│ contratos · comandos · queries · validação · observabilidade│
├──────────────────────────────────────────────────────────────┤
│                     DOMÍNIO / SERVIÇOS                       │
│ agregados · políticas · idempotência · regras de negócio    │
├──────────────────────────────────────────────────────────────┤
│                  EVENTOS / MENSAGERIA                        │
│ envelopes · sequência · retry · deduplicação · replay       │
├──────────────────────────────────────────────────────────────┤
│                    RESILIÊNCIA                               │
│ timeout · backoff · jitter · circuit breaker · bulkhead     │
├──────────────────────────────────────────────────────────────┤
│                    SISTEMAS / EMBEDDED                       │
│ parsing · buffers limitados · atomics · ownership · CRC     │
└──────────────────────────────────────────────────────────────┘
```

---

## Política pública

Pode ser publicado:

- código genérico e reproduzível;
- algoritmos e estruturas de dados;
- protocolos sintéticos;
- simuladores sem acesso a hardware real;
- testes determinísticos;
- documentação de arquitetura;
- benchmarks reproduzíveis;
- exemplos sem segredos.

Não deve ser publicado:

- empresas ou marcas privadas;
- credenciais;
- tokens;
- chaves privadas;
- endpoints de produção;
- dados de clientes;
- localização operacional;
- inventário real de dispositivos;
- topologia privada de rede;
- comandos de controle de hardware real;
- informações confidenciais.

---

<div align="center">

### Sistemas bem projetados continuam compreensíveis quando algo falha.

</div>
