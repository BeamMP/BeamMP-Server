#[macro_use] extern crate log;
#[macro_use] extern crate async_trait;
#[macro_use] extern crate lazy_static;

use std::sync::Arc;
use tokio::sync::mpsc;

mod server;
mod config;
mod heartbeat;

#[tokio::main]
async fn main() {
    // pretty_env_logger::formatted_timed_builder().filter_level(log::LevelFilter::max()).init();
    pretty_env_logger::formatted_timed_builder().filter_level(log::LevelFilter::Debug).init();

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

    let user_config = Arc::new(user_config);

    server_main(user_config).await;
}

async fn server_main(user_config: Arc<config::Config>) {
    let (hb_tx, hb_rx) = mpsc::channel(100);

    tokio::spawn(heartbeat::backend_heartbeat(user_config.clone(), hb_rx));

    let mut server = server::Server::new(user_config)
        .await
        .map_err(|e| error!("{:?}", e))
        .expect("Failed to start server!");

    let mut status = server.get_server_status();
    hb_tx.send(status.clone()).await;
    loop {
        // TODO: Error handling
        if server.clients.len() > 0 {
            tokio::select! {
                ret = server::read_tcp(&mut server.clients) => {
                    match ret {
                        Ok(ret) => if let Some((index, packet)) = ret {
                            server.process_tcp(index, packet).await;
                        },
                        Err(e) => error!("Error: {e}"),
                    }
                }
                ret = server::read_udp(&mut server.udp_socket) => {
                    if let Some((addr, packet)) = ret {
                        server.process_udp(addr, packet).await;
                    }
                }
                _ = tokio::time::sleep(tokio::time::Duration::from_millis(50)) => {}
            }
        } else {
            // TODO: Scuffed?
            tokio::time::sleep(tokio::time::Duration::from_millis(150)).await;
        }

        if let Err(e) = server.process().await {
            error!("{:?}", e);
        }

        let new_status = server.get_server_status();

        if status != new_status {
            trace!("WHAT");
            status = new_status;
            hb_tx.send(status.clone()).await;
        }
    }
}
