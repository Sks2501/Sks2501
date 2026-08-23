type WorkflowStatus = "draft" | "active" | "suspended";

type EventMetadata = Readonly<{
  eventId: string;
  commandId: string;
  correlationId: string;
  occurredAt: string;
}>;

type WorkflowCreated = Readonly<{
  type: "workflow.created";
  workflowId: string;
  name: string;
  metadata: EventMetadata;
}>;

type WorkflowRenamed = Readonly<{
  type: "workflow.renamed";
  workflowId: string;
  name: string;
  metadata: EventMetadata;
}>;

type WorkflowActivated = Readonly<{
  type: "workflow.activated";
  workflowId: string;
  metadata: EventMetadata;
}>;

type WorkflowSuspended = Readonly<{
  type: "workflow.suspended";
  workflowId: string;
  reason: string;
  metadata: EventMetadata;
}>;

type DomainEvent =
  | WorkflowCreated
  | WorkflowRenamed
  | WorkflowActivated
  | WorkflowSuspended;

type StoredEvent = Readonly<{
  streamId: string;
  version: number;
  event: DomainEvent;
}>;

type AppendReceipt = Readonly<{
  streamId: string;
  previousVersion: number;
  currentVersion: number;
  eventIds: readonly string[];
  idempotentReplay: boolean;
}>;

class ConcurrencyError extends Error {
  constructor(
    readonly streamId: string,
    readonly expectedVersion: number,
    readonly actualVersion: number,
  ) {
    super(
      `Conflito de concorrência no stream ${streamId}: esperado=${expectedVersion}, atual=${actualVersion}`,
    );
    this.name = "ConcurrencyError";
  }
}

class IdempotencyConflictError extends Error {
  constructor(readonly commandId: string) {
    super(`commandId reutilizado com operação incompatível: ${commandId}`);
    this.name = "IdempotencyConflictError";
  }
}

class DomainRuleError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "DomainRuleError";
  }
}

class InMemoryEventStore {
  private readonly streams = new Map<string, StoredEvent[]>();
  private readonly commandReceipts = new Map<
    string,
    Readonly<{ streamId: string; fingerprint: string; receipt: AppendReceipt }>
  >();

  load(streamId: string): readonly StoredEvent[] {
    const stream = this.streams.get(streamId) ?? [];
    return stream.map((entry) => ({ ...entry, event: structuredClone(entry.event) }));
  }

  append(
    streamId: string,
    expectedVersion: number,
    commandId: string,
    events: readonly DomainEvent[],
  ): AppendReceipt {
    if (!streamId.trim()) {
      throw new Error("streamId vazio");
    }
    if (!commandId.trim()) {
      throw new Error("commandId vazio");
    }
    if (events.length === 0) {
      throw new Error("append sem eventos");
    }

    const fingerprint = JSON.stringify(
      events.map((event) => ({ type: event.type, workflowId: event.workflowId })),
    );
    const previousReceipt = this.commandReceipts.get(commandId);

    if (previousReceipt) {
      if (
        previousReceipt.streamId !== streamId ||
        previousReceipt.fingerprint !== fingerprint
      ) {
        throw new IdempotencyConflictError(commandId);
      }
      return { ...previousReceipt.receipt, idempotentReplay: true };
    }

    const current = this.streams.get(streamId) ?? [];
    const actualVersion = current.length;

    if (actualVersion !== expectedVersion) {
      throw new ConcurrencyError(streamId, expectedVersion, actualVersion);
    }

    const staged: StoredEvent[] = events.map((event, index) => ({
      streamId,
      version: actualVersion + index + 1,
      event: structuredClone(event),
    }));

    const next = [...current, ...staged];
    this.streams.set(streamId, next);

    const receipt: AppendReceipt = {
      streamId,
      previousVersion: actualVersion,
      currentVersion: next.length,
      eventIds: staged.map((entry) => entry.event.metadata.eventId),
      idempotentReplay: false,
    };

    this.commandReceipts.set(commandId, {
      streamId,
      fingerprint,
      receipt,
    });

    return receipt;
  }
}

type WorkflowSnapshot = Readonly<{
  id: string | null;
  name: string | null;
  status: WorkflowStatus | null;
  suspensionReason: string | null;
  version: number;
}>;

class WorkflowAggregate {
  private id: string | null = null;
  private name: string | null = null;
  private status: WorkflowStatus | null = null;
  private suspensionReason: string | null = null;
  private version = 0;

  static rehydrate(events: readonly StoredEvent[]): WorkflowAggregate {
    const aggregate = new WorkflowAggregate();
    for (const stored of events) {
      aggregate.apply(stored.event);
      aggregate.version = stored.version;
    }
    return aggregate;
  }

  snapshot(): WorkflowSnapshot {
    return {
      id: this.id,
      name: this.name,
      status: this.status,
      suspensionReason: this.suspensionReason,
      version: this.version,
    };
  }

  decideCreate(
    workflowId: string,
    name: string,
    metadata: EventMetadata,
  ): readonly DomainEvent[] {
    if (this.id !== null) {
      throw new DomainRuleError("workflow já existe");
    }
    const normalizedName = name.trim();
    if (normalizedName.length < 3 || normalizedName.length > 80) {
      throw new DomainRuleError("nome deve ter entre 3 e 80 caracteres");
    }
    return [
      {
        type: "workflow.created",
        workflowId,
        name: normalizedName,
        metadata,
      },
    ];
  }

  decideRename(name: string, metadata: EventMetadata): readonly DomainEvent[] {
    this.assertExists();
    const normalizedName = name.trim();
    if (normalizedName.length < 3 || normalizedName.length > 80) {
      throw new DomainRuleError("nome deve ter entre 3 e 80 caracteres");
    }
    if (normalizedName === this.name) {
      return [];
    }
    return [
      {
        type: "workflow.renamed",
        workflowId: this.id as string,
        name: normalizedName,
        metadata,
      },
    ];
  }

  decideActivate(metadata: EventMetadata): readonly DomainEvent[] {
    this.assertExists();
    if (this.status === "active") {
      return [];
    }
    return [
      {
        type: "workflow.activated",
        workflowId: this.id as string,
        metadata,
      },
    ];
  }

  decideSuspend(
    reason: string,
    metadata: EventMetadata,
  ): readonly DomainEvent[] {
    this.assertExists();
    if (this.status !== "active") {
      throw new DomainRuleError("somente workflow ativo pode ser suspenso");
    }
    const normalizedReason = reason.trim();
    if (normalizedReason.length < 3 || normalizedReason.length > 160) {
      throw new DomainRuleError("motivo deve ter entre 3 e 160 caracteres");
    }
    return [
      {
        type: "workflow.suspended",
        workflowId: this.id as string,
        reason: normalizedReason,
        metadata,
      },
    ];
  }

  private assertExists(): void {
    if (this.id === null) {
      throw new DomainRuleError("workflow inexistente");
    }
  }

  private apply(event: DomainEvent): void {
    switch (event.type) {
      case "workflow.created":
        this.id = event.workflowId;
        this.name = event.name;
        this.status = "draft";
        this.suspensionReason = null;
        return;
      case "workflow.renamed":
        this.name = event.name;
        return;
      case "workflow.activated":
        this.status = "active";
        this.suspensionReason = null;
        return;
      case "workflow.suspended":
        this.status = "suspended";
        this.suspensionReason = event.reason;
        return;
    }
  }
}

type CreateWorkflow = Readonly<{
  type: "create";
  commandId: string;
  correlationId: string;
  workflowId: string;
  name: string;
}>;

type RenameWorkflow = Readonly<{
  type: "rename";
  commandId: string;
  correlationId: string;
  workflowId: string;
  name: string;
}>;

type ActivateWorkflow = Readonly<{
  type: "activate";
  commandId: string;
  correlationId: string;
  workflowId: string;
}>;

type SuspendWorkflow = Readonly<{
  type: "suspend";
  commandId: string;
  correlationId: string;
  workflowId: string;
  reason: string;
}>;

type Command =
  | CreateWorkflow
  | RenameWorkflow
  | ActivateWorkflow
  | SuspendWorkflow;

class IdGenerator {
  private sequence = 0;

  next(prefix: string): string {
    this.sequence += 1;
    return `${prefix}-${this.sequence.toString().padStart(6, "0")}`;
  }
}

class CommandHandler {
  constructor(
    private readonly store: InMemoryEventStore,
    private readonly ids: IdGenerator,
  ) {}

  execute(command: Command): AppendReceipt | null {
    const streamId = `workflow:${command.workflowId}`;
    const stored = this.store.load(streamId);
    const aggregate = WorkflowAggregate.rehydrate(stored);
    const metadata: EventMetadata = {
      eventId: this.ids.next("evt"),
      commandId: command.commandId,
      correlationId: command.correlationId,
      occurredAt: new Date().toISOString(),
    };

    let events: readonly DomainEvent[];
    switch (command.type) {
      case "create":
        events = aggregate.decideCreate(command.workflowId, command.name, metadata);
        break;
      case "rename":
        events = aggregate.decideRename(command.name, metadata);
        break;
      case "activate":
        events = aggregate.decideActivate(metadata);
        break;
      case "suspend":
        events = aggregate.decideSuspend(command.reason, metadata);
        break;
    }

    if (events.length === 0) {
      return null;
    }

    return this.store.append(
      streamId,
      aggregate.snapshot().version,
      command.commandId,
      events,
    );
  }

  query(workflowId: string): WorkflowSnapshot {
    return WorkflowAggregate.rehydrate(
      this.store.load(`workflow:${workflowId}`),
    ).snapshot();
  }
}

function printReceipt(label: string, receipt: AppendReceipt | null): void {
  console.log(label, receipt ?? { status: "sem_mudanca" });
}

function main(): void {
  const store = new InMemoryEventStore();
  const ids = new IdGenerator();
  const handler = new CommandHandler(store, ids);
  const workflowId = "wf-sintetico-001";

  const createCommand: CreateWorkflow = {
    type: "create",
    commandId: "cmd-001",
    correlationId: "corr-001",
    workflowId,
    name: "Pipeline Sintético",
  };

  printReceipt("create", handler.execute(createCommand));
  printReceipt("create_replay", handler.execute(createCommand));

  printReceipt(
    "activate",
    handler.execute({
      type: "activate",
      commandId: "cmd-002",
      correlationId: "corr-001",
      workflowId,
    }),
  );

  printReceipt(
    "rename",
    handler.execute({
      type: "rename",
      commandId: "cmd-003",
      correlationId: "corr-002",
      workflowId,
      name: "Pipeline Sintético V2",
    }),
  );

  printReceipt(
    "suspend",
    handler.execute({
      type: "suspend",
      commandId: "cmd-004",
      correlationId: "corr-003",
      workflowId,
      reason: "manutenção sintética programada",
    }),
  );

  const finalSnapshot = handler.query(workflowId);
  console.log("snapshot_final", finalSnapshot);

  const streamId = `workflow:${workflowId}`;
  const staleVersion = store.load(streamId).length - 1;

  try {
    store.append(streamId, staleVersion, "cmd-conflito", [
      {
        type: "workflow.renamed",
        workflowId,
        name: "Alteração concorrente",
        metadata: {
          eventId: ids.next("evt"),
          commandId: "cmd-conflito",
          correlationId: "corr-conflito",
          occurredAt: new Date().toISOString(),
        },
      },
    ]);
  } catch (error: unknown) {
    if (error instanceof ConcurrencyError) {
      console.log("concorrencia_detectada", {
        streamId: error.streamId,
        expectedVersion: error.expectedVersion,
        actualVersion: error.actualVersion,
      });
    } else {
      throw error;
    }
  }
}

main();
