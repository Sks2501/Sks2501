package main

import (
	"context"
	"errors"
	"fmt"
	"sync"
	"sync/atomic"
	"time"
)

var (
	errTransient = errors.New("falha_transitoria")
	errPermanent = errors.New("falha_permanente")
)

type breakerState uint8

const (
	stateClosed breakerState = iota
	stateOpen
	stateHalfOpen
)

type CircuitBreaker struct {
	mu               sync.Mutex
	state            breakerState
	failures         uint32
	failureThreshold uint32
	openedAt         time.Time
	resetTimeout     time.Duration
	halfOpenInFlight bool
}

func NewCircuitBreaker(threshold uint32, resetTimeout time.Duration) *CircuitBreaker {
	if threshold == 0 {
		threshold = 1
	}
	if resetTimeout <= 0 {
		resetTimeout = 250 * time.Millisecond
	}

	return &CircuitBreaker{
		state:            stateClosed,
		failureThreshold: threshold,
		resetTimeout:     resetTimeout,
	}
}

func (b *CircuitBreaker) Allow(now time.Time) bool {
	b.mu.Lock()
	defer b.mu.Unlock()

	switch b.state {
	case stateClosed:
		return true
	case stateOpen:
		if now.Sub(b.openedAt) < b.resetTimeout {
			return false
		}
		b.state = stateHalfOpen
		b.halfOpenInFlight = true
		return true
	case stateHalfOpen:
		if b.halfOpenInFlight {
			return false
		}
		b.halfOpenInFlight = true
		return true
	default:
		return false
	}
}

func (b *CircuitBreaker) Success() {
	b.mu.Lock()
	defer b.mu.Unlock()

	b.state = stateClosed
	b.failures = 0
	b.halfOpenInFlight = false
}

func (b *CircuitBreaker) Failure(now time.Time) {
	b.mu.Lock()
	defer b.mu.Unlock()

	switch b.state {
	case stateHalfOpen:
		b.state = stateOpen
		b.failures = b.failureThreshold
		b.openedAt = now
		b.halfOpenInFlight = false
	case stateClosed:
		b.failures++
		if b.failures >= b.failureThreshold {
			b.state = stateOpen
			b.openedAt = now
		}
	case stateOpen:
		// Mantém o instante original de abertura para permitir transição a half-open.
	}
}

func (b *CircuitBreaker) Snapshot() (string, uint32) {
	b.mu.Lock()
	defer b.mu.Unlock()

	var name string
	switch b.state {
	case stateClosed:
		name = "closed"
	case stateOpen:
		name = "open"
	case stateHalfOpen:
		name = "half_open"
	default:
		name = "unknown"
	}
	return name, b.failures
}

type Task struct {
	ID      int
	Payload string
}

type Result struct {
	TaskID    int
	WorkerID  int
	Attempts  int
	Err       error
	Latency   time.Duration
	Processed bool
}

type Metrics struct {
	started   atomic.Uint64
	succeeded atomic.Uint64
	failed    atomic.Uint64
	retries   atomic.Uint64
	blocked   atomic.Uint64
}

func deterministicJitter(taskID, attempt int) time.Duration {
	value := (taskID*31 + attempt*17) % 29
	return time.Duration(value) * time.Millisecond
}

func backoff(taskID, attempt int) time.Duration {
	shift := attempt
	if shift > 6 {
		shift = 6
	}
	base := 20 * time.Millisecond * time.Duration(1<<shift)
	return base + deterministicJitter(taskID, attempt)
}

func simulatedDependency(task Task, attempt int) error {
	if task.Payload == "" {
		return errPermanent
	}
	if task.ID%113 == 0 {
		return errPermanent
	}
	if task.ID%19 == 0 && attempt < 2 {
		return errTransient
	}
	return nil
}

func waitContext(ctx context.Context, delay time.Duration) error {
	timer := time.NewTimer(delay)
	defer timer.Stop()

	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return nil
	}
}

func executeWithResilience(
	ctx context.Context,
	task Task,
	breaker *CircuitBreaker,
	metrics *Metrics,
) (int, error) {
	const maxAttempts = 5
	attempts := 0

	for attempts < maxAttempts {
		if err := ctx.Err(); err != nil {
			return attempts, err
		}

		if !breaker.Allow(time.Now()) {
			metrics.blocked.Add(1)
			if err := waitContext(ctx, 25*time.Millisecond); err != nil {
				return attempts, err
			}
			continue
		}

		err := simulatedDependency(task, attempts)
		attempts++

		if err == nil {
			breaker.Success()
			return attempts, nil
		}

		if errors.Is(err, errPermanent) {
			return attempts, err
		}

		breaker.Failure(time.Now())

		if attempts >= maxAttempts {
			return attempts, err
		}

		metrics.retries.Add(1)
		if err := waitContext(ctx, backoff(task.ID, attempts-1)); err != nil {
			return attempts, err
		}
	}

	return attempts, errTransient
}

func worker(
	ctx context.Context,
	workerID int,
	tasks <-chan Task,
	results chan<- Result,
	breaker *CircuitBreaker,
	metrics *Metrics,
	wg *sync.WaitGroup,
) {
	defer wg.Done()

	for {
		select {
		case <-ctx.Done():
			return
		case task, ok := <-tasks:
			if !ok {
				return
			}

			metrics.started.Add(1)
			started := time.Now()
			attempts, err := executeWithResilience(ctx, task, breaker, metrics)
			latency := time.Since(started)

			if err == nil {
				metrics.succeeded.Add(1)
			} else {
				metrics.failed.Add(1)
			}

			result := Result{
				TaskID:    task.ID,
				WorkerID:  workerID,
				Attempts:  attempts,
				Err:       err,
				Latency:   latency,
				Processed: true,
			}

			select {
			case <-ctx.Done():
				return
			case results <- result:
			}
		}
	}
}

func main() {
	const (
		workerCount = 6
		totalTasks  = 240
		queueSize   = 32
	)

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	tasks := make(chan Task, queueSize)
	results := make(chan Result, queueSize)
	breaker := NewCircuitBreaker(4, 180*time.Millisecond)
	metrics := &Metrics{}

	var wg sync.WaitGroup
	wg.Add(workerCount)
	for workerID := 0; workerID < workerCount; workerID++ {
		go worker(ctx, workerID, tasks, results, breaker, metrics, &wg)
	}

	go func() {
		defer close(tasks)
		for id := 1; id <= totalTasks; id++ {
			task := Task{
				ID:      id,
				Payload: fmt.Sprintf("evento-sintetico-%06d", id),
			}

			select {
			case <-ctx.Done():
				return
			case tasks <- task:
			}
		}
	}()

	go func() {
		wg.Wait()
		close(results)
	}()

	startedAt := time.Now()
	received := 0

	for result := range results {
		received++
		if result.Err != nil || result.Attempts > 1 {
			status := "ok"
			if result.Err != nil {
				status = result.Err.Error()
			}
			fmt.Printf(
				"task=%d worker=%d tentativas=%d status=%s latencia_ms=%d\n",
				result.TaskID,
				result.WorkerID,
				result.Attempts,
				status,
				result.Latency.Milliseconds(),
			)
		}
	}

	breakerState, breakerFailures := breaker.Snapshot()
	duration := time.Since(startedAt)

	fmt.Println("\n=== métricas ===")
	fmt.Printf("resultados=%d\n", received)
	fmt.Printf("iniciados=%d\n", metrics.started.Load())
	fmt.Printf("sucessos=%d\n", metrics.succeeded.Load())
	fmt.Printf("falhas=%d\n", metrics.failed.Load())
	fmt.Printf("retries=%d\n", metrics.retries.Load())
	fmt.Printf("bloqueios_circuit_breaker=%d\n", metrics.blocked.Load())
	fmt.Printf("breaker_estado=%s\n", breakerState)
	fmt.Printf("breaker_falhas_consecutivas=%d\n", breakerFailures)
	fmt.Printf("duracao_ms=%d\n", duration.Milliseconds())

	if err := ctx.Err(); err != nil && !errors.Is(err, context.Canceled) {
		fmt.Printf("contexto=%v\n", err)
	}
}
