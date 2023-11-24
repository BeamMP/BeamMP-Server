use std::path::{Path, PathBuf};
use serde::Deserialize;
use uuid::Uuid;
use crate::fs_util;

#[derive(Deserialize)]
pub struct Config {
    #[serde(skip)] // Skipping uses Default::default, which makes a new vector for us :)
    pub mods: Vec<(String, usize)>,

    #[serde(rename = "General")]
    pub general: GeneralSettings,
}

#[derive(Deserialize)]
pub struct GeneralSettings {
    #[serde(rename = "Port")]
    pub port: Option<u16>,

    #[serde(rename = "Name")]
    pub name: String,

    #[serde(rename = "Description")]
    pub description: String,

    #[serde(rename = "AuthKey")]
    pub auth_key: Option<String>,

    #[serde(rename = "MaxCars")]
    pub max_cars: Option<u8>,

    #[serde(rename = "MaxPlayers")]
    pub max_players: usize,

    #[serde(rename = "Private")]
    pub private: bool,

    #[serde(rename = "Map")]
    pub map: String,

    // Options below are not yet supported
    #[serde(rename = "LogChat")]
    pub log_chat: bool,
    #[serde(rename = "Debug")]
    pub debug: bool,
    #[serde(rename = "ResourceFolder")]
    pub resource_folder: String,
}

impl GeneralSettings {
    pub fn is_auth_key_valid(&self) -> bool {
        if let Some(auth_key) = &self.auth_key {
            Uuid::parse_str(auth_key.as_str()).is_ok()
        } else {
            false
        }
    }

    /// Returns the client resource path, and ensures it exists.
    /// Default is Resources/Client.
    pub fn get_client_resource_folder(&self) -> anyhow::Result<String> {
        let res_client_path = Path::new(self.resource_folder.as_str()).join("Client");
        fs_util::ensure_path_exists(&res_client_path)?;
        Ok(fs_util::path_to_string(res_client_path))
    }

    /// Returns the server resource path, and ensures it exists.
    /// Default is Resources/Server.
    pub fn get_server_resource_folder(&self) -> anyhow::Result<String> {
        let res_server_path = Path::new(self.resource_folder.as_str()).join("Server");
        fs_util::ensure_path_exists(&res_server_path)?;
        Ok(fs_util::path_to_string(res_server_path))
    }
}
