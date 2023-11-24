#[macro_use] extern crate log;
#[macro_use] extern crate async_trait;
#[macro_use] extern crate lazy_static;

use std::path::Path;
use argh::FromArgs;

use std::sync::Arc;
use tokio::sync::mpsc;

mod logger;
mod tui;
mod server;
mod config;
mod heartbeat;
mod fs_util;

#[derive(FromArgs)]
/// BeamMP Server v3.3.0
struct Args {
    /// disables the TUI and shows a simple console log instead
    #[argh(switch)]
    disable_tui: bool,
}

#[tokio::main]
async fn main() {
    let args: Args = argh::from_env();

    if !args.disable_tui {
        logger::init(log::LevelFilter::max()).expect("Failed to enable logger!");
    } else {
        // pretty_env_logger::formatted_timed_builder().filter_level(log::LevelFilter::max()).init();
        pretty_env_logger::formatted_timed_builder().filter_level(log::LevelFilter::Debug).init();
    }

    let mut user_config: config::Config = toml::from_str(
            &std::fs::read_to_string("ServerConfig.toml")
                .map_err(|_| error!("Failed to read config file!"))
                .expect("Failed to read config file!")
        )
        .map_err(|_| error!("Failed to parse config file!"))
        .expect("Failed to parse config file!");


    let client_resources = user_config.general
        .get_client_resource_folder()
        .expect("Failed to create the client resource folder");

    for entry in std::fs::read_dir(client_resources).expect("Failed to read client resource folder!") {
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

    let (cmd_tx, cmd_rx) = mpsc::channel(100);

    if !args.disable_tui {
        tokio::spawn(tui::tui_main(user_config.clone(), cmd_tx));
    }

    server_main(user_config, cmd_rx).await;
}

async fn server_main(user_config: Arc<config::Config>, mut cmd_rx: mpsc::Receiver<Vec<String>>) {
    let (hb_tx, hb_rx) = mpsc::channel(100);

    tokio::spawn(heartbeat::backend_heartbeat(user_config.clone(), hb_rx));

    let mut server = server::Server::new(user_config)
        .await
        .map_err(|e| error!("{:?}", e))
        .expect("Failed to start server!");

    let mut status = server.get_server_status();
    hb_tx.send(status.clone()).await;
    'server: loop {
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
            status = new_status;
            hb_tx.send(status.clone()).await;
        }

        // Process commands
        match cmd_rx.try_recv() {
            Ok(cmd) => if cmd.len() > 0 {
                match cmd[0].as_str() {
                    "exit" => {
                        server.close().await;
                        break 'server;
                    },
                    "players" => {
                        let mut pl = "Players:\n".to_string();
                        for (i, (id, player)) in status.player_list.iter().enumerate() {
                            pl.push_str(&format!("\t[{: >2}] - {player}", id));
                            if i + 1 < status.player_list.len() {
                                pl.push('\n');
                            }
                        }
                        info!("{}", pl);
                    },
                    _ => info!("Unknown command!"),
                }
            } else {
                // what!
            },
            Err(mpsc::error::TryRecvError::Empty) => {},
            Err(e) => {
                error!("Error: {e}");
                break;
            },
        }
    }
}
