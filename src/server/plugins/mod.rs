pub mod backend_lua;

use std::sync::Arc;
use tokio::runtime::Runtime;
use tokio::sync::Mutex;
use tokio::sync::mpsc::{self, Sender, Receiver};
use tokio::sync::oneshot;

/// NOTE: Send is required as the backend is constructed on the main thread and sent over.
///       Even if we construct it inside the runtime however, because of tokio, we would
//        still have to require Send as the runtime might run on different threads (?)
pub trait Backend: Send {
    fn load(&mut self, code: String) -> anyhow::Result<()>;
    fn load_api(&mut self, tx: Arc<Sender<ServerBoundPluginEvent>>) -> anyhow::Result<()>;

    fn call_event_handler(&mut self, event: ScriptEvent, args: Vec<Argument>);
}

// TODO: This is quite focused on Lua right now, perhaps in the future we want to modify this list
//       to be more versatile?
#[derive(Debug)]
pub enum Argument {
    String(String),
    Boolean(bool),
    Number(f32),
}

#[derive(Debug)]
pub enum ScriptEvent {
    OnPluginLoaded,
}

#[derive(Debug)]
pub enum PluginBoundPluginEvent {
    CallEventHandler((ScriptEvent, Vec<Argument>)),
    PlayerCount(usize),
}

#[derive(Debug)]
pub enum ServerBoundPluginEvent {
    PluginLoaded,

    /// Arguments: (event name, handler function name)
    RegisterEventHandler((String, String)),
    RequestPlayerCount(oneshot::Sender<PluginBoundPluginEvent>),
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
                    match message {
                        PluginBoundPluginEvent::CallEventHandler((event, args)) => {
                            backend.call_event_handler(event, args);
                        },
                        _ => {},
                    }
                } else {
                    error!("Event receiver has closed!"); // TODO: We probably want to display the plugin name here too lol
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

    // TODO: For performance I think we can turn this into an iterator instead of first allocating
    //       a full vector?
    pub fn get_events(&mut self) -> Vec<ServerBoundPluginEvent> {
        let mut events = Vec::new();
        loop {
            match self.rx.try_recv() {
                Ok(event) => events.push(event),
                Err(mpsc::error::TryRecvError::Disconnected) => break, // TODO: This means the runtime is dead!!! Handle this!!!
                Err(_) => break,
            }
        }
        events
    }

    // TODO: Handle error when connection is closed, as it means the runtime is down
    pub async fn send_event(&self, event: PluginBoundPluginEvent) {
        let _ = self.tx.send(event).await;
    }
}
