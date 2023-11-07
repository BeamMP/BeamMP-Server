#[macro_use] extern crate async_trait;
#[macro_use] extern crate log;

mod server;
mod config;

#[tokio::main]
async fn main() {
    let user_config: config::Config = toml::from_str(
        &std::fs::read_to_string("ServerConfig.toml")
            .map_err(|_| error!("Failed to read config file!"))
            .expect("Failed to read config file!")
        )
        .map_err(|_| error!("Failed to parse config file!"))
        .expect("Failed to parse config file!");

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
