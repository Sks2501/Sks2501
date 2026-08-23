<div align="center">

# Sks2501 Engineering

### Embedded Systems · Distributed Systems · Protocol Design · Backend Infrastructure

![Systems](https://img.shields.io/badge/systems-engineering-0b1220?style=for-the-badge)
![Embedded](https://img.shields.io/badge/embedded-C%20%7C%20RTOS-0b1220?style=for-the-badge)
![Protocols](https://img.shields.io/badge/protocols-versioned-0b1220?style=for-the-badge)
![Backend](https://img.shields.io/badge/backend-TypeScript%20%7C%20Node.js-0b1220?style=for-the-badge)

</div>

---

## Engineering scope

This profile is intentionally focused on technical work rather than personal information.

Primary areas:

- embedded software architecture;
- deterministic state machines;
- binary and application protocol design;
- telemetry pipelines and event schemas;
- distributed backend services;
- API contracts and compatibility policies;
- observability, resilience and fault isolation;
- secure-by-default public sandboxes;
- hardware/software integration research;
- developer tooling and diagnostics.

---

## Public engineering work

### VOE LAB Prototypes

A public systems-engineering laboratory containing safe, synthetic implementations of:

- protocol specifications;
- OpenAPI contracts;
- telemetry envelopes;
- deterministic simulators;
- architecture decision records;
- threat models;
- validation schemas;
- embedded reference codecs;
- interoperability rules;
- failure-mode documentation.

> Public repositories contain synthetic data and non-production interfaces only. No credentials, private infrastructure, real device identifiers or operational control paths are published.

---

## Engineering principles

```text
explicit contracts
    > implicit behavior

bounded failure domains
    > global failure

versioned protocols
    > undocumented coupling

observable systems
    > invisible state

reproducible builds
    > environment-dependent behavior

safe public interfaces
    > exposed operational internals
```

---

## System layers

```text
┌─────────────────────────────────────────────────────────────┐
│                    APPLICATION LAYER                        │
│  API contracts · dashboards · operators · client software  │
├─────────────────────────────────────────────────────────────┤
│                    DOMAIN / SERVICES                        │
│  state machines · policies · scheduling · orchestration    │
├─────────────────────────────────────────────────────────────┤
│                  EVENT / TELEMETRY BUS                      │
│  envelopes · schemas · sequencing · idempotency · replay   │
├─────────────────────────────────────────────────────────────┤
│                    PROTOCOL LAYER                           │
│  framing · validation · compatibility · integrity checks   │
├─────────────────────────────────────────────────────────────┤
│                    EMBEDDED LAYER                           │
│  C · deterministic parsers · bounded memory · diagnostics  │
└─────────────────────────────────────────────────────────────┘
```

---

## Technical focus

| Domain | Practices |
|---|---|
| Embedded | bounded buffers, explicit ownership, deterministic execution, state machines |
| Protocols | version negotiation, framing, integrity validation, compatibility matrices |
| Backend | strict typing, idempotency, timeouts, retries, circuit breaking, rate limiting |
| Data | JSON Schema, event envelopes, correlation IDs, sequence numbers, auditability |
| Security | least privilege, fail-closed behavior, secret isolation, threat modeling |
| Reliability | health models, graceful degradation, fault containment, recovery semantics |
| Tooling | CI validation, static analysis, contract checks, reproducible local simulations |

---

## Public repository policy

Public code is designed to be inspectable without exposing operational systems.

Published material may include:

- reusable protocol concepts;
- synthetic simulators;
- parsers and codecs operating on fictional frames;
- validation schemas;
- architecture documentation;
- non-routable sandbox examples;
- deterministic tests and fixtures.

Not published:

- production secrets;
- private hostnames;
- customer information;
- real fleet identifiers;
- operational credentials;
- production control commands;
- confidential infrastructure topology.

---

<div align="center">

### Systems should be understandable under failure, not only when everything works.

</div>
