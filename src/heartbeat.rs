use serde::Serialize;
use tokio::sync::mpsc::Receiver;

#[derive(Serialize)]
struct HeartbeatInfo {
    uuid: String,
    players: usize,
    maxplayers: usize,
    port: u16,
    map: String,
    private: String, // Needs to be either "true" or "false"
    version: String,
    clientversion: String,
    name: String,
    modlist: String,
    modstotalsize: usize,
    modstotal: usize,
    playerslist: String,
    desc: String,
}

pub async fn backend_heartbeat(config: std::sync::Arc<crate::config::Config>, mut hb_rx: Receiver<crate::server::ServerStatus>) {
    let mut info = HeartbeatInfo {
        uuid: config.general.auth_key.clone().unwrap_or(String::from("Unknown name!")),
        players: 0,
        maxplayers: config.general.max_players,
        port: config.general.port.unwrap_or(30814),
        map: config.general.map.clone(),
        private: if config.general.private { String::from("true") } else { String::from("false") },
        version: String::from("3.3.0"), // TODO: Don't hardcode this
        clientversion: String::from("2.0"), // TODO: What? I think for now I can fill in 2.0
        name: config.general.name.clone(),
        modlist: String::from("-"), // TODO: Implement this
        modstotalsize: 0, // TODO: Implement this
        modstotal: 0, // TODO: Implement this
        playerslist: String::new(),
        desc: config.general.description.clone(),
    };

    let mut interval = tokio::time::interval(tokio::time::Duration::from_secs(30));
    loop {
        interval.tick().await;

        tokio::select! {
            _ = heartbeat_post(&info) => {}
            status = hb_rx.recv() => {
                if let Some(status) = status {
                    trace!("status update: {:?}", status);
                    info.players = status.player_count;
                    info.playerslist = status.player_list.clone();
                }
            }
        }
    }
}

async fn heartbeat_post(heartbeat_info: &HeartbeatInfo) {
    match reqwest::Client::builder()
        .local_address("0.0.0.0".parse::<std::net::IpAddr>().unwrap())
        .build().unwrap()
        .post("https://backend.beammp.com/heartbeat")
        .form(heartbeat_info)
        .send()
        .await
    {
        Ok(resp) => {
            debug!("heartbeat response:\n{:?}", resp.text().await);
        },
        Err(e) => error!("Heartbeat error occured: {e}"),
    }
}
