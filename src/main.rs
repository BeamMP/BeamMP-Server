#[macro_use] extern crate log;
#[macro_use] extern crate async_trait;
#[macro_use] extern crate lazy_static;

use serde::Serialize;

mod server;
mod config;

#[tokio::main]
async fn main() {
    pretty_env_logger::formatted_timed_builder().filter_level(log::LevelFilter::max()).init();
    // pretty_env_logger::formatted_timed_builder().filter_level(log::LevelFilter::Info).init();

    let mut user_config: config::Config = toml::from_str(
        &std::fs::read_to_string("ServerConfig.toml")
            .map_err(|_| error!("Failed to read config file!"))
            .expect("Failed to read config file!")
        )
        .map_err(|_| error!("Failed to parse config file!"))
        .expect("Failed to parse config file!");

    // TODO: This should not error lol
    for entry in std::fs::read_dir("Resources/Client").expect("Failed to read Resources/Client!") {
        if let Ok(entry) = entry {
            if entry.path().is_file() {
                if let Ok(metadata) = entry.metadata() {
                    if let Some(filename) = entry.path().file_name().map(|s| s.to_string_lossy()) {
                        let mut name = filename.to_string();
                        if !name.starts_with("/") {
                            name = format!("/{name}");
                        }
                        user_config.mods.push((name, metadata.len() as usize));
                    }
                }
            }
        }
    }

    debug!("Mods: {:?}", user_config.mods);

    let user_config = std::sync::Arc::new(user_config);

    tokio::spawn(backend_heartbeat(user_config.clone()));

    let mut server = server::Server::new(user_config)
        .await
        .map_err(|e| error!("{:?}", e))
        .expect("Failed to start server!");

    loop {
        if let Err(e) = server.process().await {
            error!("{:?}", e);
        }
    }
}

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

async fn backend_heartbeat(config: std::sync::Arc<config::Config>) {
    let info = HeartbeatInfo {
        uuid: config.general.auth_key.clone().unwrap_or(String::from("Unknown name!")),
        players: 0, // TODO: Implement this. Easiest would probably be to have the server send updates every so often
        maxplayers: config.general.max_players,
        port: config.general.port.unwrap_or(30814),
        map: config.general.map.clone(),
        private: if config.general.private { String::from("true") } else { String::from("false") },
        version: String::from("1.0"),
        clientversion: String::from("2.0"), // TODO: What? I think for now I can fill in 2.0
        name: config.general.name.clone(),
        modlist: String::from("-"), // TODO: Implement this.
        modstotalsize: 0, // TODO: Implement this.
        modstotal: 0, // TODO: Implement this.
        playerslist: String::from("luuk-bepis;"), // TODO: Implement this
        desc: config.general.description.clone(),
    };

    let mut interval = tokio::time::interval(tokio::time::Duration::from_secs(30));
    loop {
        interval.tick().await;

        heartbeat_post(&info).await;
    }
}

async fn heartbeat_post(heartbeat_info: &HeartbeatInfo) {
    if let Err(e) = reqwest::Client::new()
        .post("https://backend.beammp.com/heartbeat")
        .form(heartbeat_info)
        .send()
        .await
    {
        error!("Heartbeat error occured: {e}");
    }
}
