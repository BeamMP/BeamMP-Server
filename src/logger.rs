use log::{Record, Level, Metadata, SetLoggerError, LevelFilter};
use std::sync::Mutex;

lazy_static! {
    static ref LOG_BUFFER: Mutex<Vec<(Level, String)>> = Mutex::new(Vec::new());
}

struct BufferedLogger;

impl log::Log for BufferedLogger {
    fn enabled(&self, _metadata: &Metadata) -> bool {
        true
    }

    fn log(&self, record: &Record) {
        if self.enabled(record.metadata()) {
            let msg = format!("{}", record.args());
            let mut lock = LOG_BUFFER.lock().expect("Logging lock poisoned!");
            lock.push((record.level(), msg));
        }
    }

    fn flush(&self) {}
}

static LOGGER: BufferedLogger = BufferedLogger;

pub fn init(filter: LevelFilter) -> Result<(), SetLoggerError> {
    log::set_logger(&LOGGER)
        .map(|()| log::set_max_level(filter))
}

pub async fn drain_log_buffer() -> Vec<(Level, String)> {
    let mut lock = LOG_BUFFER.lock().expect("Logging lock poisoned!");
    let v = lock.drain(..).collect();
    drop(lock);
    v
}
