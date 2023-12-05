pub mod backend_lua;

use std::sync::Arc;
use std::collections::HashMap;
use tokio::runtime::Runtime;
use tokio::sync::mpsc::{self, Sender, Receiver};
use tokio::sync::oneshot;

/// NOTE: Send is required as the backend is constructed on the main thread and sent over.
///       Even if we construct it inside the runtime however, because of tokio, we would
//        still have to require Send as the runtime might run on different threads (?)
pub trait Backend: Send {
    fn load(&mut self, code: String) -> anyhow::Result<()>;
    fn load_api(&mut self, tx: Arc<Sender<ServerBoundPluginEvent>>) -> anyhow::Result<()>;

    fn call_event_handler(&mut self, event: ScriptEvent, resp: Option<oneshot::Sender<Argument>>);
}

// TODO: This is quite focused on Lua right now, perhaps in the future we want to modify this list
//       to be more versatile?
#[derive(Debug, Clone)]
pub enum Argument {
    String(String),
    Boolean(bool),
    Number(f32),
    Integer(i64),
    Table(HashMap<String, Argument>),
}

#[derive(Debug)]
pub struct PlayerIdentifiers {
    pub ip: String,
    pub beammp_id: String,
}

impl PlayerIdentifiers {
    pub fn to_map(&self) -> HashMap<String, Argument> {
        let mut m = HashMap::new();
        m.insert(String::from("ip"), Argument::String(self.ip.clone()));
        m.insert(String::from("beammp"), Argument::String(self.beammp_id.clone()));
        m
    }
}

#[derive(Debug)]
pub struct PositionRaw {
    pub pos: [f32; 3],
    pub rot: [f32; 4],
}

impl PositionRaw {
    pub fn to_map(&self) -> HashMap<String, Argument> {
        let mut pm = HashMap::new();
        pm.insert(String::from("x"), Argument::Number(self.pos[0]));
        pm.insert(String::from("y"), Argument::Number(self.pos[1]));
        pm.insert(String::from("z"), Argument::Number(self.pos[2]));

        let mut rm = HashMap::new();
        rm.insert(String::from("x"), Argument::Number(self.rot[0]));
        rm.insert(String::from("y"), Argument::Number(self.rot[1]));
        rm.insert(String::from("z"), Argument::Number(self.rot[2]));
        rm.insert(String::from("w"), Argument::Number(self.rot[3]));

        let mut m = HashMap::new();
        m.insert(String::from("pos"), Argument::Table(pm));
        m.insert(String::from("rot"), Argument::Table(rm));
        m
    }
}

#[derive(Debug)]
pub enum ScriptEvent {
    OnPluginLoaded,
    OnShutdown,

    // TODO: Because of limitations in how we accept clients, onPlayerAuth runs after mod downloading and syncing
    //       has already happened. With some reworking, this can be avoided, but it's not easy.
    //       This is quite important however, because otherwise mod downloads will get triggered
    //       constantly even when someone isn't allowed to join!
    OnPlayerAuthenticated { name: String, role: String, is_guest: bool, identifiers: PlayerIdentifiers },
    // TODO: See the comment above, this results in these 2 events being useless for now.
    //       I've simply left them in for backwards compatibility for now!
    OnPlayerConnecting { pid: u8 },
    OnPlayerJoining { pid: u8 },

    OnPlayerDisconnect { pid: u8, name: String },

    OnVehicleSpawn { pid: u8, vid: u8, car_data: String },
    OnVehicleEdited { pid: u8, vid: u8, car_data: String },
    OnVehicleDeleted { pid: u8, vid: u8 },
    OnVehicleReset { pid: u8, vid: u8, car_data: String },

    OnChatMessage { pid: u8, name: String, message: String },
}

#[derive(Debug)]
pub enum PluginBoundPluginEvent {
    None,

    CallEventHandler((ScriptEvent, Option<oneshot::Sender<Argument>>)),

    PlayerCount(usize),
    Players(HashMap<u8, String>),
    PlayerIdentifiers(PlayerIdentifiers),

    PlayerVehicles(HashMap<u8, String>),
    PositionRaw(PositionRaw),
}

// TODO: Perhaps it would be nice to ensure each sender can only sned specifically what it needs to.
//       Purely to ensure type safety and slightly cleaner code on the backend implementation side.
#[derive(Debug)]
pub enum ServerBoundPluginEvent {
    PluginLoaded,

    RequestPlayerCount(oneshot::Sender<PluginBoundPluginEvent>),
    RequestPlayers(oneshot::Sender<PluginBoundPluginEvent>),
    RequestPlayerIdentifiers((u8, oneshot::Sender<PluginBoundPluginEvent>)),

    RequestPlayerVehicles((u8, oneshot::Sender<PluginBoundPluginEvent>)),
    RequestPositionRaw((u8, u8, oneshot::Sender<PluginBoundPluginEvent>)),

    SendChatMessage((isize, String)),
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
                        PluginBoundPluginEvent::CallEventHandler((event, resp)) => {
                            backend.call_event_handler(event, resp);
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

    pub async fn close(mut self) {
        let (tx, mut rx) = oneshot::channel();
        self.send_event(PluginBoundPluginEvent::CallEventHandler((ScriptEvent::OnShutdown, Some(tx)))).await;
        let _ = rx.await; // We just wait for it to finish shutting down
        self.runtime.shutdown_background();
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
