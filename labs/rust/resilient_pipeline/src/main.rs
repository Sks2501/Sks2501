use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::mpsc::{sync_channel, Receiver, SyncSender};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

const CAPACIDADE_FILA: usize = 32;
const NUM_WORKERS: usize = 4;
const TOTAL_JOBS: u64 = 250;
const MAX_TENTATIVAS: u32 = 4;

#[derive(Debug, Clone)]
struct Job {
    id: u64,
    payload: Vec<u8>,
}

#[derive(Debug)]
struct Resultado {
    job_id: u64,
    worker_id: usize,
    tentativas: u32,
    checksum: Option<u64>,
    erro: Option<&'static str>,
    latencia: Duration,
}

#[derive(Default)]
struct Metricas {
    recebidos: AtomicU64,
    concluidos: AtomicU64,
    falhas: AtomicU64,
    retries: AtomicU64,
}

fn checksum_fowler_noll_vo_1a(bytes: &[u8]) -> u64 {
    const OFFSET: u64 = 0xcbf29ce484222325;
    const PRIME: u64 = 0x100000001b3;

    bytes.iter().fold(OFFSET, |hash, byte| {
        (hash ^ u64::from(*byte)).wrapping_mul(PRIME)
    })
}

fn executar(job: &Job, tentativa: u32) -> Result<u64, &'static str> {
    if job.payload.is_empty() {
        return Err("payload_vazio");
    }

    // Falha transitória determinística para exercitar retry sem depender de aleatoriedade.
    if job.id % 17 == 0 && tentativa < 2 {
        return Err("dependencia_transitoria");
    }

    // Falha permanente sintética para provar isolamento de falha.
    if job.id % 97 == 0 {
        return Err("entrada_rejeitada");
    }

    Ok(checksum_fowler_noll_vo_1a(&job.payload))
}

fn backoff(job_id: u64, tentativa: u32) -> Duration {
    let exponencial_ms = 10_u64.saturating_mul(1_u64 << tentativa.min(6));
    let jitter_deterministico_ms = (job_id.wrapping_mul(31) + u64::from(tentativa) * 17) % 23;
    Duration::from_millis(exponencial_ms + jitter_deterministico_ms)
}

fn processar_com_retry(job: &Job, metricas: &Metricas) -> (u32, Result<u64, &'static str>) {
    let mut ultima_falha = "falha_desconhecida";

    for tentativa in 0..MAX_TENTATIVAS {
        match executar(job, tentativa) {
            Ok(checksum) => return (tentativa + 1, Ok(checksum)),
            Err(erro) => {
                ultima_falha = erro;
                let permanente = erro == "payload_vazio" || erro == "entrada_rejeitada";
                let ultima_tentativa = tentativa + 1 >= MAX_TENTATIVAS;

                if permanente || ultima_tentativa {
                    return (tentativa + 1, Err(erro));
                }

                metricas.retries.fetch_add(1, Ordering::Relaxed);
                thread::sleep(backoff(job.id, tentativa));
            }
        }
    }

    (MAX_TENTATIVAS, Err(ultima_falha))
}

fn worker_loop(
    worker_id: usize,
    rx: Arc<Mutex<Receiver<Job>>>,
    resultado_tx: SyncSender<Resultado>,
    metricas: Arc<Metricas>,
    cancelado: Arc<AtomicBool>,
) {
    loop {
        if cancelado.load(Ordering::Acquire) {
            break;
        }

        let job = {
            let guard = match rx.lock() {
                Ok(guard) => guard,
                Err(_) => {
                    cancelado.store(true, Ordering::Release);
                    break;
                }
            };

            match guard.recv() {
                Ok(job) => job,
                Err(_) => break,
            }
        };

        metricas.recebidos.fetch_add(1, Ordering::Relaxed);
        let inicio = Instant::now();
        let (tentativas, resultado) = processar_com_retry(&job, &metricas);
        let latencia = inicio.elapsed();

        let saida = match resultado {
            Ok(checksum) => {
                metricas.concluidos.fetch_add(1, Ordering::Relaxed);
                Resultado {
                    job_id: job.id,
                    worker_id,
                    tentativas,
                    checksum: Some(checksum),
                    erro: None,
                    latencia,
                }
            }
            Err(erro) => {
                metricas.falhas.fetch_add(1, Ordering::Relaxed);
                Resultado {
                    job_id: job.id,
                    worker_id,
                    tentativas,
                    checksum: None,
                    erro: Some(erro),
                    latencia,
                }
            }
        };

        if resultado_tx.send(saida).is_err() {
            cancelado.store(true, Ordering::Release);
            break;
        }
    }
}

fn construir_payload(id: u64) -> Vec<u8> {
    format!("evento-sintetico-{id:06}-dados-deterministicos").into_bytes()
}

fn main() {
    let (job_tx, job_rx) = sync_channel::<Job>(CAPACIDADE_FILA);
    let (resultado_tx, resultado_rx) = sync_channel::<Resultado>(CAPACIDADE_FILA);

    let job_rx = Arc::new(Mutex::new(job_rx));
    let metricas = Arc::new(Metricas::default());
    let cancelado = Arc::new(AtomicBool::new(false));

    let mut workers = Vec::with_capacity(NUM_WORKERS);

    for worker_id in 0..NUM_WORKERS {
        let rx = Arc::clone(&job_rx);
        let tx = resultado_tx.clone();
        let metricas_worker = Arc::clone(&metricas);
        let cancelado_worker = Arc::clone(&cancelado);

        workers.push(thread::spawn(move || {
            worker_loop(worker_id, rx, tx, metricas_worker, cancelado_worker)
        }));
    }

    drop(resultado_tx);

    let produtor = thread::spawn(move || {
        for id in 1..=TOTAL_JOBS {
            let job = Job {
                id,
                payload: construir_payload(id),
            };

            if job_tx.send(job).is_err() {
                break;
            }
        }
    });

    let inicio_global = Instant::now();
    let mut recebidos_resultado = 0_u64;
    let mut checksum_agregado = 0_u64;

    while recebidos_resultado < TOTAL_JOBS {
        match resultado_rx.recv() {
            Ok(resultado) => {
                recebidos_resultado += 1;
                if let Some(checksum) = resultado.checksum {
                    checksum_agregado ^= checksum.rotate_left((resultado.job_id % 63) as u32);
                }

                if resultado.erro.is_some() || resultado.tentativas > 1 {
                    println!(
                        "job={} worker={} tentativas={} status={} latencia_ms={}",
                        resultado.job_id,
                        resultado.worker_id,
                        resultado.tentativas,
                        resultado.erro.unwrap_or("ok"),
                        resultado.latencia.as_millis()
                    );
                }
            }
            Err(_) => break,
        }
    }

    if produtor.join().is_err() {
        eprintln!("produtor terminou com panic");
        cancelado.store(true, Ordering::Release);
    }

    for worker in workers {
        if worker.join().is_err() {
            eprintln!("worker terminou com panic");
            cancelado.store(true, Ordering::Release);
        }
    }

    let duracao = inicio_global.elapsed();
    let recebidos = metricas.recebidos.load(Ordering::Relaxed);
    let concluidos = metricas.concluidos.load(Ordering::Relaxed);
    let falhas = metricas.falhas.load(Ordering::Relaxed);
    let retries = metricas.retries.load(Ordering::Relaxed);

    println!("\n=== métricas ===");
    println!("recebidos={recebidos}");
    println!("concluidos={concluidos}");
    println!("falhas={falhas}");
    println!("retries={retries}");
    println!("duracao_ms={}", duracao.as_millis());
    println!("checksum_agregado={checksum_agregado:016x}");

    assert_eq!(recebidos, TOTAL_JOBS);
    assert_eq!(concluidos + falhas, TOTAL_JOBS);
}
