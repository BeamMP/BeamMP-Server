use std::net::SocketAddr;
use std::sync::{Arc, Mutex, atomic::{AtomicBool, Ordering}};
use std::time::Instant;

use tokio::net::{TcpListener, UdpSocket};
use tokio::task::{JoinHandle, JoinSet};

use num_enum::IntoPrimitive;

use nalgebra::*;

mod backend;
mod car;
mod client;
mod packet;

pub use backend::*;
pub use car::*;
pub use client::*;
pub use packet::*;

pub use crate::config::Config;

#[derive(PartialEq, IntoPrimitive, Copy, Clone, Debug)]
#[repr(u8)]
enum ServerState {
    Unknown = 0,

    WaitingForClients,
    WaitingForReady,
    WaitingForSpawns,
    Qualifying,
    LiningUp,
    Countdown,
    Race,
    Finish,
}

pub struct Server {
    tcp_listener: Arc<TcpListener>,
    udp_socket: Arc<UdpSocket>,

    clients_incoming: Arc<Mutex<Vec<Client>>>,

    pub clients: Vec<Client>,

    connect_runtime_handle: JoinHandle<()>,

    config: Arc<Config>,
}

impl Server {
    pub async fn new(config: Arc<Config>) -> anyhow::Result<Self> {
        let config_ref = Arc::clone(&config);

        let port = config.general.port.unwrap_or(48900);

        let tcp_listener = {
            let bind_addr = &format!("0.0.0.0:{}", port);
            Arc::new(TcpListener::bind(bind_addr).await?)
        };
        let tcp_listener_ref = Arc::clone(&tcp_listener);

        let udp_socket = {
            let bind_addr = &format!("0.0.0.0:{}", port);
            Arc::new(UdpSocket::bind(bind_addr).await?)
        };

        let clients_incoming = Arc::new(Mutex::new(Vec::new()));
        let clients_incoming_ref = Arc::clone(&clients_incoming);
        debug!("Client acception runtime starting...");
        let connect_runtime_handle = tokio::spawn(async move {
            let mut set = JoinSet::new();
            loop {
                match tcp_listener_ref.accept().await {
                    Ok((mut socket, addr)) => {
                        info!("New client connected: {:?}", addr);

                        let cfg_ref = config_ref.clone();
                        let ci_ref = clients_incoming_ref.clone();

                        set.spawn(async move {
                            socket.set_nodelay(true); // TODO: Is this good?

                            let mut client = Client::new(socket);
                            match client.authenticate(&cfg_ref).await {
                                Ok(b) if b => {
                                    let mut lock = ci_ref
                                        .lock()
                                        .map_err(|e| error!("{:?}", e))
                                        .expect("Failed to acquire lock on mutex!");
                                    lock.push(client);
                                    drop(lock);
                                },
                                Ok(b) => {
                                    debug!("Downloader?");
                                },
                                Err(e) => {
                                    error!("Authentication error occured, kicking player...");
                                    error!("{:?}", e);
                                    client.kick("Failed to authenticate player!").await;
                                }
                            }
                        });
                    }
                    Err(e) => error!("Failed to accept incoming connection: {:?}", e),
                }

                if set.is_empty() == false {
                    tokio::select!(
                        _ = set.join_next() => {},
                        _ = tokio::time::sleep(tokio::time::Duration::from_secs(1)) => {},
                    )
                }
            }
        });
        debug!("Client acception runtime started!");

        Ok(Self {
            tcp_listener: tcp_listener,
            udp_socket: udp_socket,

            clients_incoming: clients_incoming,

            clients: Vec::new(),

            connect_runtime_handle: connect_runtime_handle,

            config: config,
        })
    }

    pub async fn process(&mut self) -> anyhow::Result<()> {
        // Bit weird, but this is all to avoid deadlocking the server if anything goes wrong
        // with the client acception runtime. If that one locks, the server won't accept
        // more clients, but it will at least still process all other clients
        let mut joined_names = Vec::new();
        if let Ok(mut clients_incoming_lock) = self.clients_incoming.try_lock() {
            if clients_incoming_lock.len() > 0 {
                trace!(
                    "Accepting {} incoming clients...",
                    clients_incoming_lock.len()
                );
                for i in 0..clients_incoming_lock.len() {
                    joined_names.push(
                        clients_incoming_lock[i]
                            .info
                            .as_ref()
                            .unwrap()
                            .username
                            .clone(),
                    );
                    self.clients.push(clients_incoming_lock.swap_remove(i));
                }
                trace!("Accepted incoming clients!");
            }
        }

        // Process UDP packets
        // TODO: Use a UDP addr -> client ID look up table
        for (addr, packet) in self.read_udp_packets().await {
            if packet.data.len() == 0 {
                continue;
            }
            let id = packet.data[0] - 1; // Offset by 1
            let data = packet.data[2..].to_vec();
            let packet_processed = RawPacket {
                header: data.len() as u32,
                data,
            };
            'search: for i in 0..self.clients.len() {
                if self.clients[i].id == id {
                    self.parse_packet_udp(i, addr, packet_processed).await?;
                    break 'search;
                }
            }
        }

        // Process all the clients (TCP)
        for i in 0..self.clients.len() {
            if let Some(client) = self.clients.get_mut(i) {
                match client.process().await {
                    Ok(packet_opt) => {
                        if let Some(raw_packet) = packet_opt {
                            self.parse_packet(i, raw_packet.clone()).await?;;
                        }
                    }
                    Err(e) => client.kick(&format!("Kicked: {:?}", e)).await,
                }

                // More efficient than broadcasting as we are already looping
                for name in joined_names.iter() {
                    self.clients[i]
                        .queue_packet(Packet::Notification(NotificationPacket::new(format!(
                            "Welcome {}!",
                            name.to_string()
                        ))))
                        .await;
                }
            }
        }

        // I'm sorry for this code :(
        for i in 0..self.clients.len() {
            if self.clients.get(i).ok_or(ServerError::ClientDoesntExist)?.state == ClientState::Disconnect {
                let id = self.clients.get(i).ok_or(ServerError::ClientDoesntExist)?.id;
                for j in 0..self.clients.get(i).ok_or(ServerError::ClientDoesntExist)?.cars.len() {
                    let car_id = self.clients.get(i).ok_or(ServerError::ClientDoesntExist)?.cars[j].0;
                    let delete_packet = format!("Od:{}-{}", id, car_id);
                    self.broadcast(Packet::Raw(RawPacket::from_str(&delete_packet)), None)
                        .await;
                }
                info!("Disconnecting client {}...", id);
                self.clients.remove(i);
                info!("Client {} disconnected!", id);
            }
        }

        Ok(())
    }

    async fn broadcast(&self, packet: Packet, owner: Option<u8>) {
        for client in &self.clients {
            if let Some(id) = owner {
                if id == client.id {
                    continue;
                }
            }
            client.queue_packet(packet.clone()).await;
        }
    }

    async fn broadcast_udp(&self, packet: Packet, owner: Option<u8>) {
        for client in &self.clients {
            if let Some(id) = owner {
                if id == client.id {
                    continue;
                }
            }
            // client.queue_packet(packet.clone()).await;
            if let Some(udp_addr) = client.udp_addr {
                self.send_udp(udp_addr, &packet).await;
            }
        }
    }

    async fn send_udp(&self, udp_addr: SocketAddr, packet: &Packet) {
        let data = packet.get_data();
        if data.len() > 400 {
            trace!("Compressing...");
            let mut compressed: Vec<u8> = Vec::with_capacity(100_000);
            let mut compressor = flate2::Compress::new(flate2::Compression::best(), true);
            if let Err(e) = compressor.compress_vec(
                data,
                &mut compressed,
                flate2::FlushCompress::Sync,
            ) {
                error!("Compression failed!");
                return;
            }
            let mut new_data = "ABG:".as_bytes()[..4].to_vec();
            new_data.append(&mut compressed);
            if let Err(e) = self.udp_socket.try_send_to(&new_data, udp_addr) {
                error!("UDP Packet send error: {:?}", e);
            }
        } else {
            if let Err(e) = self.udp_socket.try_send_to(&data, udp_addr) {
                error!("UDP Packet send error: {:?}", e);
            }
        }
    }

    async fn read_udp_packets(&self) -> Vec<(SocketAddr, RawPacket)> {
        let mut packets = Vec::new();
        'read: loop {
            let mut data = vec![0u8; 4096];
            let data_size;
            let data_addr;

            match self.udp_socket.try_recv_from(&mut data) {
                Ok((0, _)) => {
                    error!("UDP socket is readable, yet has 0 bytes to read!");
                    break 'read;
                }
                Ok((n, addr)) => (data_size, data_addr) = (n, addr),
                Err(_) => break 'read,
            }

            let packet = RawPacket {
                header: data_size as u32,
                data: data[..data_size].to_vec(),
            };
            packets.push((data_addr, packet));
        }
        packets
    }

    async fn parse_packet_udp(
        &mut self,
        client_idx: usize,
        udp_addr: SocketAddr,
        mut packet: RawPacket,
    ) -> anyhow::Result<()> {
        if packet.data.len() > 0 {
            let client = &mut self.clients[client_idx];
            let client_id = client.get_id();

            client.udp_addr = Some(udp_addr);

            // Check if compressed
            let mut is_compressed = false;
            if packet.data.len() > 3 {
                let string_data = String::from_utf8_lossy(&packet.data[..4]);
                if string_data.starts_with("ABG:") {
                    is_compressed = true;
                    trace!("Packet is compressed!");
                }
            }

            if is_compressed {
                let compressed = &packet.data[4..];
                let mut decompressed: Vec<u8> = Vec::with_capacity(100_000);
                let mut decompressor = flate2::Decompress::new(true);
                decompressor.decompress_vec(
                    compressed,
                    &mut decompressed,
                    flate2::FlushDecompress::Finish,
                )?;
                packet.header = decompressed.len() as u32;
                packet.data = decompressed;
                // let string_data = String::from_utf8_lossy(&packet.data[..]);
                // debug!("Unknown packet - String data: `{}`; Array: `{:?}`; Header: `{:?}`", string_data, packet.data, packet.header);
            }

            // Check packet identifier
            let packet_identifier = packet.data[0] as char;
            if packet.data[0] >= 86 && packet.data[0] <= 89 {
                self.broadcast_udp(Packet::Raw(packet), Some(client_id))
                    .await;
            } else {
                match packet_identifier {
                    'p' => {
                        self.send_udp(udp_addr, &Packet::Raw(RawPacket::from_code('p')))
                            .await;
                    }
                    'Z' => {
                        if packet.data.len() < 7 {
                            error!("Position packet too small!");
                            return Err(ServerError::BrokenPacket.into());
                        } else {
                            // Sent as text so removing 48 brings it from [48-57] to [0-9]
                            let client_id = packet.data[3] - 48;
                            let car_id = packet.data[5] - 48;

                            let pos_json = &packet.data[7..];
                            let pos_data: TransformPacket =
                                serde_json::from_str(&String::from_utf8_lossy(pos_json))?;

                            let p = Packet::Raw(packet);

                            for i in 0..self.clients.len() {
                                if self.clients[i].id == client_id {
                                    let client = &mut self.clients[i];
                                    let car = client
                                        .get_car_mut(car_id)
                                        .ok_or(ServerError::CarDoesntExist)?;
                                    car.pos = pos_data.pos.into();
                                    car.rot = Quaternion::new(
                                        pos_data.rot[3],
                                        pos_data.rot[0],
                                        pos_data.rot[1],
                                        pos_data.rot[2],
                                    );
                                    car.vel = pos_data.vel.into();
                                    car.rvel = pos_data.rvel.into();
                                    car.tim = pos_data.tim;
                                    car.ping = pos_data.ping;
                                    car.last_pos_update = Some(Instant::now());
                                } else {
                                    if let Some(udp_addr) = self.clients[i].udp_addr {
                                        self.send_udp(udp_addr, &p).await;
                                    }
                                }
                            }
                        }
                    }
                    _ => {
                        let string_data = String::from_utf8_lossy(&packet.data[..]);
                        debug!(
                            "Unknown packet UDP - String data: `{}`; Array: `{:?}`; Header: `{:?}`",
                            string_data, packet.data, packet.header
                        );
                    }
                }
            }
        }
        Ok(())
    }

    async fn parse_packet(
        &mut self,
        client_idx: usize,
        mut packet: RawPacket,
    ) -> anyhow::Result<()> {
        if packet.data.len() > 0 {
            let client_id = {
                let client = &mut self.clients[client_idx];
                client.get_id()
            };

            // Check if compressed
            let mut is_compressed = false;
            if packet.data.len() > 3 {
                let string_data = String::from_utf8_lossy(&packet.data[..4]);
                if string_data.starts_with("ABG:") {
                    is_compressed = true;
                    // trace!("Packet is compressed!");
                }
            }

            if is_compressed {
                let compressed = &packet.data[4..];
                let mut decompressed: Vec<u8> = Vec::with_capacity(100_000);
                let mut decompressor = flate2::Decompress::new(true);
                decompressor.decompress_vec(
                    compressed,
                    &mut decompressed,
                    flate2::FlushDecompress::Finish,
                )?;
                packet.header = decompressed.len() as u32;
                packet.data = decompressed;
                // let string_data = String::from_utf8_lossy(&packet.data[..]);
                // debug!("Unknown packet - String data: `{}`; Array: `{:?}`; Header: `{:?}`", string_data, packet.data, packet.header);
            }

            // Check packet identifier
            if packet.data[0] >= 86 && packet.data[0] <= 89 {
                self.broadcast(Packet::Raw(packet), Some(client_id)).await;
            } else {
                let packet_identifier = packet.data[0] as char;
                match packet_identifier {
                    'H' => {
                        // Full sync with server
                        self.clients[client_idx]
                            .queue_packet(Packet::Raw(RawPacket::from_str(&format!(
                                "Sn{}",
                                self.clients[client_idx]
                                    .info
                                    .as_ref()
                                    .unwrap()
                                    .username
                                    .clone()
                            ))))
                            .await;

                        // TODO: Sync all existing cars on server (this code is broken)
                        for client in &self.clients {
                            let pid = client.id as usize;
                            if pid != client_idx {
                                let role = client.get_roles();
                                for (vid, car) in &client.cars {
                                    self.clients[client_idx]
                                        .queue_packet(Packet::Raw(RawPacket::from_str(&format!(
                                            "Os:{role}:{}:{pid}-{vid}:{}",
                                            client.get_name(),
                                            car.car_json,
                                        ))))
                                        .await;
                                }
                            }
                        }
                    }
                    'O' => self.parse_vehicle_packet(client_idx, packet).await?,
                    'C' => {
                        // TODO: Chat filtering?
                        let packet_data = packet.data_as_string();
                        let message = packet_data.split(":").collect::<Vec<&str>>().get(2).map(|s| s.to_string()).unwrap_or(String::new());
                        let message = message.trim();
                        if message.starts_with("!") {
                            if message == "!ready" {
                                self.clients[client_idx].ready = true;
                                self.clients[client_idx].queue_packet(Packet::Raw(RawPacket::from_str("C:Server:You are now ready!"))).await;
                            } else if message == "!pos" {
                                let car = &self.clients[client_idx].cars.get(0).ok_or(ServerError::CarDoesntExist)?.1;
                                trace!("car transform (pos/rot/vel/rvel): {:?}", (car.pos, car.rot, car.vel, car.rvel));
                            } else {
                                self.clients[client_idx].queue_packet(Packet::Raw(RawPacket::from_str("C:Server:Unknown command!"))).await;
                            }
                        } else {
                            self.broadcast(Packet::Raw(packet), None).await;
                        }
                    }
                    _ => {
                        let string_data = String::from_utf8_lossy(&packet.data[..]);
                        debug!(
                            "Unknown packet - String data: `{}`; Array: `{:?}`; Header: `{:?}`",
                            string_data, packet.data, packet.header
                        );
                    }
                }
            }
        }
        Ok(())
    }

    async fn parse_vehicle_packet(
        &mut self,
        client_idx: usize,
        packet: RawPacket,
    ) -> anyhow::Result<()> {
        if packet.data.len() < 6 {
            error!("Vehicle packet too small!");
            return Ok(()); // TODO: Return error here
        }
        let code = packet.data[1] as char;
        match code {
            's' => {
                let client = &mut self.clients[client_idx];
                let mut allowed = true;
                if let Some(max_cars) = self.config.general.max_cars {
                    if client.cars.len() >= max_cars as usize { allowed = false; }
                }
                // trace!("Packet string: `{}`", packet.data_as_string());
                let split_data = packet
                    .data_as_string()
                    .splitn(3, ':')
                    .map(|s| s.to_string())
                    .collect::<Vec<String>>();
                let car_json_str = &split_data.get(2).ok_or(std::fmt::Error)?;
                // let car_json: serde_json::Value = serde_json::from_str(&car_json_str)?;
                let car_id = client.register_car(Car::new(car_json_str.to_string()));
                let client_id = client.get_id();
                if allowed {
                    client.trigger_client_event("GetSize", client_id.to_string()).await;
                    let packet_data = format!(
                        "Os:{}:{}:{}-{}:{}",
                        client.get_roles(),
                        client.get_name(),
                        client_id,
                        car_id,
                        car_json_str
                    );
                    let response = RawPacket::from_str(&packet_data);
                    self.broadcast(Packet::Raw(response), None).await;
                    info!("Spawned car for client #{}!", client_id);
                } else {
                    let packet_data = format!(
                        "Os:{}:{}:{}-{}:{}",
                        client.get_roles(),
                        client.get_name(),
                        client_id,
                        car_id,
                        car_json_str
                    );
                    let response = RawPacket::from_str(&packet_data);
                    client.write_packet(Packet::Raw(response)).await;
                    let packet_data = format!(
                        "Od:{}-{}",
                        client_id,
                        car_id,
                    );
                    let response = RawPacket::from_str(&packet_data);
                    client.write_packet(Packet::Raw(response)).await;
                    client.unregister_car(car_id);
                    info!("Blocked spawn for client #{}!", client_id);
                }
            }
            'c' => {
                // let split_data = packet.data_as_string().splitn(3, ':').map(|s| s.to_string()).collect::<Vec<String>>();
                // let car_json_str = &split_data.get(2).ok_or(std::fmt::Error)?;
                let client_id = packet.data[3] - 48;
                let car_id = packet.data[5] - 48;
                let car_json = String::from_utf8_lossy(&packet.data[7..]).to_string();
                let response = Packet::Raw(packet.clone());
                for i in 0..self.clients.len() {
                    if self.clients[i].id == client_id {
                        if let Some(car) = self.clients[i].get_car_mut(car_id) {
                            car.car_json = car_json.clone();
                        }
                    } else {
                        // Already looping so more efficient to send here
                        // if let Some(udp_addr) = self.clients[i].udp_addr {
                        //     self.write_udp(udp_addr, &response).await;
                        // }
                        self.clients[i].write_packet(response.clone()).await;
                    }
                }
            }
            'd' => {
                debug!("packet: {:?}", packet);
                let split_data = packet
                    .data_as_string()
                    .splitn(3, [':', '-'])
                    .map(|s| s.to_string())
                    .collect::<Vec<String>>();
                let client_id = split_data[1].parse::<u8>()?;
                let car_id = split_data[2].parse::<u8>()?;
                for i in 0..self.clients.len() {
                    if self.clients[i].id == client_id {
                        self.clients[i].unregister_car(car_id);
                    }
                    // Don't broadcast, we are already looping anyway
                    // if let Some(udp_addr) = self.clients[i].udp_addr {
                    //     self.send_udp(udp_addr, &Packet::Raw(packet.clone())).await;
                    // }
                    self.clients[i].write_packet(Packet::Raw(packet.clone())).await;
                }
                info!("Deleted car for client #{}!", client_id);
            }
            'r' => {
                self.broadcast(Packet::Raw(packet), Some(self.clients[client_idx].id)).await;
            }
            't' => {
                self.broadcast(Packet::Raw(packet), Some(self.clients[client_idx].id))
                    .await;
            }
            'm' => {
                self.broadcast(Packet::Raw(packet), None).await;
            }
            _ => error!("Unknown vehicle related packet!\n{:?}", packet), // TODO: Return error here
        }
        Ok(())
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        // Not sure how needed this is but it seems right?
        self.connect_runtime_handle.abort();
    }
}

#[derive(Debug)]
pub enum ServerError {
    BrokenPacket,
    CarDoesntExist,
    ClientDoesntExist,
}

impl std::fmt::Display for ServerError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(f, "{:?}", self)?;
        Ok(())
    }
}

impl std::error::Error for ServerError {}
