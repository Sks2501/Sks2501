#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <thread>
#include <type_traits>

namespace lab {

template <typename T, std::size_t Capacity>
class SpscRingBuffer final {
    static_assert(Capacity >= 2, "Capacity deve ser >= 2");
    static_assert(std::is_trivially_copyable_v<T>, "T deve ser trivialmente copiável");

public:
    SpscRingBuffer() noexcept = default;
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    [[nodiscard]] bool try_push(const T& value) noexcept {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = increment(head);

        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        storage_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> try_pop() noexcept {
        const auto tail = tail_.load(std::memory_order_relaxed);

        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }

        const T value = storage_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return value;
    }

    [[nodiscard]] bool empty() const noexcept {
        return tail_.load(std::memory_order_acquire) ==
               head_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t capacity() const noexcept {
        return Capacity - 1;
    }

private:
    static constexpr std::size_t increment(std::size_t value) noexcept {
        return (value + 1) % Capacity;
    }

    alignas(64) std::array<T, Capacity> storage_{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

} // namespace lab

struct Evento final {
    std::uint64_t sequence;
    std::uint64_t value;
};

int main() {
    constexpr std::uint64_t total = 1'000'000;
    lab::SpscRingBuffer<Evento, 4096> queue;

    std::atomic<bool> producer_done{false};
    std::atomic<bool> failed{false};
    std::atomic<std::uint64_t> consumed_count{0};
    std::atomic<std::uint64_t> consumed_sum{0};

    const auto started = std::chrono::steady_clock::now();

    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= total; ++i) {
            const Evento event{.sequence = i, .value = i * 3};

            while (!queue.try_push(event)) {
                std::this_thread::yield();
            }
        }

        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        std::uint64_t expected_sequence = 1;
        std::uint64_t local_count = 0;
        std::uint64_t local_sum = 0;

        for (;;) {
            if (auto event = queue.try_pop()) {
                if (event->sequence != expected_sequence) {
                    failed.store(true, std::memory_order_release);
                    return;
                }

                ++expected_sequence;
                ++local_count;
                local_sum += event->value;
                continue;
            }

            if (producer_done.load(std::memory_order_acquire) && queue.empty()) {
                break;
            }

            std::this_thread::yield();
        }

        consumed_count.store(local_count, std::memory_order_release);
        consumed_sum.store(local_sum, std::memory_order_release);
    });

    producer.join();
    consumer.join();

    const auto finished = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(finished - started);

    const std::uint64_t expected_sum = 3 * (total * (total + 1) / 2);
    const auto count = consumed_count.load(std::memory_order_acquire);
    const auto sum = consumed_sum.load(std::memory_order_acquire);

    if (failed.load(std::memory_order_acquire)) {
        std::cerr << "erro=sequencia_incorreta\n";
        return 1;
    }

    if (count != total) {
        std::cerr << "erro=contagem_incorreta recebido=" << count << " esperado=" << total << '\n';
        return 2;
    }

    if (sum != expected_sum) {
        std::cerr << "erro=soma_incorreta recebido=" << sum << " esperado=" << expected_sum << '\n';
        return 3;
    }

    const double seconds = static_cast<double>(elapsed.count()) / 1'000'000.0;
    const double throughput = seconds > 0.0 ? static_cast<double>(total) / seconds : 0.0;

    std::cout << "status=ok\n";
    std::cout << "capacidade_util=" << queue.capacity() << '\n';
    std::cout << "eventos=" << count << '\n';
    std::cout << "soma=" << sum << '\n';
    std::cout << "tempo_us=" << elapsed.count() << '\n';
    std::cout << "throughput_eventos_por_segundo=" << static_cast<std::uint64_t>(throughput) << '\n';

    return 0;
}
