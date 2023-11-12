pub mod backend_lua;

use tokio::runtime::Runtime;
use tokio::sync::mpsc::{self, Sender, Receiver};

pub trait Backend {
    fn load(&mut self, code: String) -> anyhow::Result<()>;
    fn load_api(&mut self) -> anyhow::Result<()> { Ok(()) }
}

#[derive(Debug)]
pub enum PluginBoundPluginEvent {

}

#[derive(Debug)]
pub enum ServerBoundPluginEvent {
    PluginLoaded,
}

pub struct Plugin {
    runtime: Runtime,
    tx: Sender<PluginBoundPluginEvent>,
    rx: Receiver<ServerBoundPluginEvent>,
}

impl Plugin {
    pub fn new(backend: Box<dyn Backend>) -> Self {
        let runtime = Runtime::new().expect("Failed to create a tokio Runtime!");
        let (pb_tx, mut pb_rx) = mpsc::channel(1_000);
        let (sb_tx, sb_rx) = mpsc::channel(1_000);
        runtime.spawn(async move {
            if sb_tx.send(ServerBoundPluginEvent::PluginLoaded).await.is_err() {
                error!("Plugin communication channels somehow already closed!");
                return;
            }
            loop {
                if let Some(message) = pb_rx.recv().await {
                    debug!("Received message: {:?}", message);
                } else {
                    return;
                }
            }
        });
        Self {
            runtime,
            tx: pb_tx,
            rx: sb_rx,
        }
    }
}
