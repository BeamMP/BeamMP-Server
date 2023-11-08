#[macro_use] extern crate log;
#[macro_use] extern crate async_trait;
#[macro_use] extern crate lazy_static;

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
                        user_config.mods.push((filename.to_string(), metadata.len() as usize));
                    }
                }
            }
        }
    }

    debug!("Mods: {:?}", user_config.mods);

    let user_config = std::sync::Arc::new(user_config);

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
