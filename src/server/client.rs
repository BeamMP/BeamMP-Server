use std::collections::HashMap;
use std::net::SocketAddr;
use std::ops::DerefMut;
use std::sync::atomic::{AtomicU8, Ordering};
use std::sync::Arc;
use std::time::Instant;

use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{
    tcp::{OwnedReadHalf, OwnedWriteHalf},
    TcpStream,
};
use tokio::sync::mpsc::{Receiver, Sender};
use tokio::sync::Mutex;
use tokio::task::JoinHandle;

use nalgebra::*;

use serde::Deserialize;

use super::backend::*;
use super::car::*;
use super::packet::*;

static ATOMIC_ID_COUNTER: AtomicU8 = AtomicU8::new(0);

lazy_static! {
    static ref CLIENT_MOD_PROGRESS: Mutex<HashMap<usize, usize>> = Mutex::new(HashMap::new());
}

#[derive(PartialEq)]
pub enum ClientState {
    None,
    Connecting,
    SyncingResources,
    Syncing,
    Disconnect,
}

#[derive(Deserialize, Debug, PartialEq, Clone)]
pub struct UserData {
    pub createdAt: String,
    pub guest: bool,
    pub roles: String,
    pub username: String,
}

pub struct Client {
    pub id: u8,
    pub udp_addr: Option<SocketAddr>,

    socket: OwnedReadHalf,
    write_half: Arc<Mutex<OwnedWriteHalf>>,
    write_runtime: JoinHandle<()>,
    write_runtime_sender: Sender<Packet>,

    pub state: ClientState,
    pub info: Option<UserData>,
    pub cars: Vec<(u8, Car)>,
}

impl Drop for Client {
    fn drop(&mut self) {
        self.write_runtime.abort();
    }
}

impl Client {
    pub fn new(socket: TcpStream) -> Self {
        let id = ATOMIC_ID_COUNTER.fetch_add(1, Ordering::SeqCst);
        trace!("Client with ID #{} created!", id);

        let (read_half, write_half) = socket.into_split();
        let (tx, mut rx): (Sender<Packet>, Receiver<Packet>) = tokio::sync::mpsc::channel(128);
        let write_half = Arc::new(Mutex::new(write_half));
        let write_half_ref = Arc::clone(&write_half);
        let handle: JoinHandle<()> = tokio::spawn(async move {
            loop {
                if let Some(packet) = rx.recv().await {
                    // trace!("Runtime received packet...");
                    let mut lock = write_half_ref.lock().await;
                    // trace!("Runtime sending packet!");
                    if let Err(e) = tcp_write(lock.deref_mut(), packet).await {
                        error!("{:?}", e);
                    };
                    // trace!("Runtime sent packet!");
                    drop(lock);
                }
            }
        });

        Self {
            id: id,
            udp_addr: None,

            socket: read_half,
            write_half: write_half,
            write_runtime: handle,
            write_runtime_sender: tx,

            state: ClientState::Connecting,
            info: None,
            cars: Vec::new(),
        }
    }

    pub async fn authenticate(&mut self, config: &super::Config) -> anyhow::Result<bool> {
        debug!("Authenticating client {}...", self.id);

        'waiting_for_c: loop {
            self.socket.readable().await?;
            let mut tmp = vec![0u8; 1];
            while self.socket.peek(&mut tmp).await? == 0 {}
            // Authentication works a little differently than normal
            // Not sure why, but the BeamMP source code shows they
            // also only read a single byte during authentication
            let code = self.read_raw(1).await?[0];
            debug!("code: '{}' / {}", code as char, code);
            match code as char {
                'C' => {
                    // We now delete existing data for this client ID, just in case.
                    // TODO: This seems like a recipe for disaster
                    let mut lock = CLIENT_MOD_PROGRESS.lock().await;
                    lock.remove(&(self.id as usize));

                    // TODO: Check client version
                    trace!("Client version packet");
                    self.socket.readable().await?;
                    let packet = self.read_packet_waiting().await?;
                    debug!("{:?}", packet);
                    break 'waiting_for_c;
                }
                'D' => {
                    // The download sequence is so awful
                    // It currently requires us to track what the next file is that
                    // we need to provide, which is hard to do with the current
                    // server design.
                    // I think I will simply keep a counter around that will
                    // track what the next mod is per client.
                    // TODO: Clean this up. It also needs to be moved out of the client code IMO

                    let id = self.read_raw(1).await?[0] as usize;
                    debug!("HandleDownload connection for client id: {}", id);

                    let mut mod_name = {
                        let mut lock = CLIENT_MOD_PROGRESS.lock().await;
                        if lock.get(&id).is_none() { lock.insert(id, 0); }
                        let next_id = lock.get_mut(&id).unwrap();

                        let bmod = &config.mods[*next_id]; // TODO: This is a bit uhh yeah
                        debug!("Mod name: {}", bmod.0);

                        *next_id += 1;

                        if *next_id >= config.mods.len() {
                            // I think this is where the connection should be closed, instead of after
                            // just 1 mod.
                        }

                        bmod.0.clone()
                    };

                    if mod_name.starts_with("/") == false {
                        mod_name = format!("/{mod_name}");
                    }

                    let mod_path = format!("Resources/Client{mod_name}");
                    let file_data = std::fs::read(mod_path)?;

                    let packet = RawPacket::from_data(file_data[(file_data.len()/2)..].to_vec());

                    {
                        let mut lock = self.write_half.lock().await;
                        lock.writable().await?;
                        trace!("Sending packets!");
                        if let Err(e) = tcp_write_raw(lock.deref_mut(), Packet::Raw(packet)).await {
                            error!("{:?}", e);
                        }
                        trace!("Packets sent!");
                        drop(lock);
                    }

                    // return Err(ClientError::IsDownloader.into());
                    return Ok(false);
                }
                _ => {
                    error!("Unknown code: {}", code);
                    return Err(ClientError::AuthenticateError.into());
                }
            }
        }

        self.write_packet(Packet::Raw(RawPacket::from_code('S')))
            .await?;

        self.socket.readable().await?;
        if let Some(packet) = self.read_packet_waiting().await? {
            debug!("packet: {:?}", packet);
            if packet.data.len() > 50 {
                self.kick("Player key too big!").await;
                return Err(ClientError::AuthenticateError.into());
            }
            let mut json = HashMap::new();
            json.insert("key".to_string(), packet.data_as_string());
            let user_data: UserData =
                authentication_request("pkToUser", json)
                    .await
                    .map_err(|e| {
                        error!("{:?}", e);
                        e
                    })?;
            debug!("user_data: {:?}", user_data);
            self.info = Some(user_data);
        } else {
            self.kick(
                "Client never sent public key! If this error persists, try restarting your game.",
            )
            .await;
        }

        self.write_packet(Packet::Raw(RawPacket::from_str(&format!("P{}", self.id))))
            .await?;

        self.state = ClientState::SyncingResources;

        debug!(
            "Authentication of client {} succesfully completed! Syncing now...",
            self.id
        );
        self.sync(config).await?;

        Ok(true)
    }

    // TODO: https://github.com/BeamMP/BeamMP-Server/blob/master/src/TNetwork.cpp#L619
    pub async fn sync(&mut self, config: &super::Config) -> anyhow::Result<()> {
        'syncing: while self.state == ClientState::SyncingResources {
            self.socket.readable().await?;
            if let Some(packet) = self.read_packet().await? {
                if packet.data.len() == 0 {
                    continue;
                }
                if packet.data.len() == 4 {
                    if packet.data == [68, 111, 110, 101] {
                        break 'syncing;
                    }
                }
                match packet.data[0] as char {
                    'S' if packet.data.len() > 1 => match packet.data[1] as char {
                        'R' => {
                            // let file_packet = if config.mods.len() == 0 {
                            //     RawPacket::from_code('-')
                            // } else {
                            //     let mut file_data = String::new();
                            //     for (name, size) in &config.mods {
                            //         let mut mod_name = name.clone();
                            //         if mod_name.starts_with("/") == false {
                            //             mod_name = format!("/{mod_name}");
                            //         }
                            //         file_data.push_str(&format!("{mod_name};"));
                            //     }
                            //     for (name, size) in &config.mods {
                            //         file_data.push_str(&format!("{size};"));
                            //     }
                            //     RawPacket::from_str(&file_data)
                            // };
                            let file_packet = RawPacket::from_code('-');
                            self.write_packet(Packet::Raw(file_packet))
                                .await?;
                        }
                        _ => error!("Unknown packet! {:?}", packet),
                    }
                    'f' => {
                        // Handle file download
                        let mut mod_name = packet.data_as_string().clone();
                        mod_name.remove(0); // Remove f
                        debug!("Client requested file {}", mod_name);

                        self.write_packet(Packet::Raw(RawPacket::from_str("AG"))).await?;

                        // Send the first half of the file
                        if mod_name.starts_with("/") == false {
                            mod_name = format!("/{mod_name}");
                        }

                        let mod_path = format!("Resources/Client{mod_name}");
                        let file_data = std::fs::read(mod_path)?;

                        let packet = RawPacket::from_data(file_data[..(file_data.len()/2)].to_vec());

                        {
                            let mut lock = self.write_half.lock().await;
                            lock.writable().await?;
                            trace!("Sending packets!");
                            if let Err(e) = tcp_write_raw(lock.deref_mut(), Packet::Raw(packet)).await {
                                error!("{:?}", e);
                            }
                            trace!("Packets sent!");
                            drop(lock);
                        }
                    }
                    _ => error!("Unknown packet! {:?}", packet),
                }
            }
        }
        self.state = ClientState::None;
        trace!("Done syncing! Sending map name...");
        self.write_packet(Packet::Raw(RawPacket::from_str(&format!(
            "M{}",
            config.general.map
        ))))
        .await?;
        trace!("Map name sent!");
        Ok(())
    }

    /// This function should never block. It should simply check if there's a
    /// packet, and then and only then should it read it. If this were to block, the server
    /// would come to a halt until this function unblocks.
    pub async fn process(&mut self) -> anyhow::Result<Option<RawPacket>> {
        if let Some(packet) = self.read_packet().await? {
            return Ok(Some(packet));
        }
        Ok(None)
    }

    pub async fn process_blocking(&mut self) -> anyhow::Result<Option<RawPacket>> {
        if let Some(packet) = self.read_packet_waiting().await? {
            return Ok(Some(packet));
        }
        Ok(None)
    }

    pub fn disconnect(&mut self) {
        self.state = ClientState::Disconnect;
    }

    pub async fn kick(&mut self, msg: &str) {
        // let _ = self.socket.writable().await;
        // let _ = self.write_packet(Packet::Raw(RawPacket::from_str(&format!("K{}", msg)))).await;
        // self.disconnect();
        let _ = self
            .write_packet(Packet::Raw(RawPacket::from_str(&format!("K{}", msg))))
            .await;
        self.disconnect();
    }

    // Panics when userdata is not set!
    pub fn get_name(&self) -> &str {
        &self.info.as_ref().unwrap().username
    }

    pub fn get_id(&self) -> u8 {
        self.id
    }

    // Panics when userdata is not set!
    pub fn get_roles(&self) -> &str {
        &self.info.as_ref().unwrap().roles
    }

    pub fn register_car(&mut self, car: Car) -> u8 {
        let mut free_num = 0;
        for (num, _) in &self.cars {
            if num == &free_num {
                free_num += 1;
            } else {
                break;
            }
        }
        self.cars.push((free_num, car));
        free_num
    }

    pub fn unregister_car(&mut self, car_id: u8) {
        let prev_len = self.cars.len();
        self.cars.retain(|(id, _)| id != &car_id);
        if prev_len == self.cars.len() {
            error!(
                "Failed to unregister car #{} for client #{}! Ignoring for now...",
                car_id, self.id
            );
        }
    }

    pub fn get_car_mut(&mut self, mut car_id: u8) -> Option<&mut Car> {
        for (num, car) in &mut self.cars {
            if num == &mut car_id {
                return Some(car);
            }
        }
        None
    }

    async fn read_raw(&mut self, count: usize) -> anyhow::Result<Vec<u8>> {
        let mut b = vec![0u8; count];
        self.socket.read_exact(&mut b).await?;
        Ok(b)
    }

    async fn read_packet_waiting(&mut self) -> anyhow::Result<Option<RawPacket>> {
        let start = std::time::Instant::now();
        'wait: loop {
            if let Some(packet) = self.read_packet().await? {
                return Ok(Some(packet));
            }
            if start.elapsed().as_secs() >= 5 {
                break 'wait;
            }
            tokio::time::sleep(std::time::Duration::from_millis(500)).await;
        }
        Err(ClientError::ConnectionTimeout.into())
    }

    /// Must be non-blocking
    async fn read_packet(&mut self) -> anyhow::Result<Option<RawPacket>> {
        let mut header = [0u8; 4];
        match self.socket.try_read(&mut header) {
            Ok(0) => {
                error!("Socket is readable, yet has 0 bytes to read! Disconnecting client...");
                self.disconnect();
                return Ok(None);
            }
            Ok(_n) => {}
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                return Ok(None);
            }
            Err(e) => return Err(e.into()),
        }

        let expected_size = u32::from_le_bytes(header) as usize;
        let mut data = Vec::new();
        let mut tmp_data = vec![0u8; expected_size];
        let mut data_size = 0;
        while data_size < expected_size {
            match self.socket.try_read(&mut tmp_data) {
                Ok(0) => {
                    error!("Socket is readable, yet has 0 bytes to read! Disconnecting client...");
                    self.disconnect();
                    return Ok(None);
                }
                Ok(n) => {
                    data_size += n;
                    data.extend_from_slice(&tmp_data[..n]);
                },
                Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                    // debug!("Packet appears to be ready, yet can't be read yet!");
                    // self.socket.read(&mut data).await?;
                    return Ok(None);
                }
                Err(e) => return Err(e.into()),
            }
        }

        Ok(Some(RawPacket {
            header: data_size as u32,
            data: data[..data_size].to_vec(),
        }))
    }

    /// Blocking write
    pub async fn write_packet(&mut self, packet: Packet) -> anyhow::Result<()> {
        let mut lock = self.write_half.lock().await;
        lock.writable().await?;
        trace!("Sending packet!");
        if let Err(e) = tcp_write(lock.deref_mut(), packet).await {
            error!("{:?}", e);
        }
        trace!("Packet sent!");
        drop(lock);
        Ok(())
    }

    pub async fn queue_packet(&self, packet: Packet) {
        // TODO: If packet gets lost, put it back at the front of the queue?
        let _ = self.write_runtime_sender.send(packet).await;
    }

    pub async fn trigger_client_event<S: Into<String>, D: Into<String>>(&self, event_name: S, data: D) {
        let event_name = event_name.into();
        let data = data.into();
        debug!("Calling client event '{}' with data '{}'", event_name, data);
        let packet_data = format!("E:{}:{}", event_name, data);
        self.queue_packet(Packet::Raw(RawPacket::from_str(&packet_data))).await;
    }
}

#[derive(Debug)]
pub enum ClientError {
    AuthenticateError,
    ConnectionTimeout,
    IsDownloader,
}

impl std::fmt::Display for ClientError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{:?}", self)?;
        Ok(())
    }
}

impl std::error::Error for ClientError {}

#[async_trait]
trait Writable {
    async fn writable(&self) -> std::io::Result<()>;
}

#[async_trait]
impl Writable for TcpStream {
    async fn writable(&self) -> std::io::Result<()> {
        self.writable().await
    }
}

#[async_trait]
impl Writable for OwnedWriteHalf {
    async fn writable(&self) -> std::io::Result<()> {
        self.writable().await
    }
}

async fn tcp_write<W: AsyncWriteExt + Writable + std::marker::Unpin>(
    w: &mut W,
    mut packet: Packet,
) -> anyhow::Result<()> {
    let compressed = match packet.get_code() {
        Some('O') => true,
        Some('T') => true,
        _ => packet.get_data().len() > 400,
    };

    if compressed {
        let mut compressed: Vec<u8> = Vec::with_capacity(100_000);
        let mut compressor = flate2::Compress::new(flate2::Compression::best(), true);
        compressor.compress_vec(
            packet.get_data(),
            &mut compressed,
            flate2::FlushCompress::Sync,
        )?;
        let mut new_data = "ABG:".as_bytes()[..4].to_vec();
        new_data.append(&mut compressed);
        packet.set_header(new_data.len() as u32);
        packet.set_data(new_data);
    }

    tcp_write_raw(w, packet).await?;

    Ok(())
}

async fn tcp_write_raw<W: AsyncWriteExt + Writable + std::marker::Unpin>(
    w: &mut W,
    mut packet: Packet,
) -> anyhow::Result<()> {
    let mut raw_data: Vec<u8> = packet.get_header().to_le_bytes().to_vec();
    raw_data.extend_from_slice(packet.get_data());
    w.writable().await?;
    w.write(&raw_data).await?;
    Ok(())
}

async fn tcp_send_file<W: AsyncWriteExt + Writable + std::marker::Unpin>(
    w: &mut W,
    file_name: String,
) -> anyhow::Result<()> {
    debug!("Sending file '{}'", file_name);
    tcp_write(w, Packet::Raw(RawPacket::from_str("KYou have not downloaded the mod manually!"))).await?;
    Ok(())
}
