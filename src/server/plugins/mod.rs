pub mod backend_lua;

use std::sync::Arc;
use tokio::runtime::Runtime;
use tokio::sync::Mutex;
use tokio::sync::mpsc::{self, Sender, Receiver};

/// NOTE: Send is required as the backend is constructed on the main thread and sent over.
///       Even if we construct it inside the runtime however, because of tokio, we would
//        still have to require Send as the runtime might run on different threads (?)
pub trait Backend: Send {
    fn load(&mut self, code: String) -> anyhow::Result<()>;
    fn load_api(&mut self, tx: Arc<Sender<ServerBoundPluginEvent>>) -> anyhow::Result<()> { Ok(()) }
}

#[derive(Debug)]
pub enum Argument {
    Number(f32),
}

#[derive(Debug)]
pub enum PluginBoundPluginEvent {
    CallEventHandler((String, Vec<Argument>))
}

#[derive(Debug)]
pub enum ServerBoundPluginEvent {
    PluginLoaded,

    /// Arguments: (event name, handler function name)
    RegisterEventHandler((String, String)),
}

pub struct Plugin {
    runtime: Runtime,
    tx: Sender<PluginBoundPluginEvent>,
    rx: Receiver<ServerBoundPluginEvent>,
}

impl Plugin {
    pub fn new(mut backend: Box<dyn Backend>, src: String) -> anyhow::Result<Self> {
        let runtime = Runtime::new().expect("Failed to create a tokio Runtime!");
        let (pb_tx, mut pb_rx) = mpsc::channel(1_000);
        let (sb_tx, sb_rx) = mpsc::channel(1_000);
        let sb_tx = Arc::new(sb_tx);
        runtime.spawn_blocking(move || {
            if backend.load_api(sb_tx.clone()).is_ok() {
                if backend.load(src).is_ok() {
                    if sb_tx.blocking_send(ServerBoundPluginEvent::PluginLoaded).is_err() {
                        error!("Plugin communication channels somehow already closed!");
                        return;
                    }
                }
            }

            loop {
                if let Some(message) = pb_rx.blocking_recv() {
                    debug!("Received message: {:?}", message);
                } else {
                    return;
                }
            }
        });
        Ok(Self {
            runtime,
            tx: pb_tx,
            rx: sb_rx,
        })
    }
}
