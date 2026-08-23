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
	de b.mu.Unlock()
}
