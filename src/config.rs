use serde::Deserialize;

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

    #[serde(rename = "MaxCars")]
    pub max_cars: Option<u8>,

    #[serde(rename = "MaxPlayers")]
    pub max_players: usize,

    #[serde(rename = "Map")]
    pub map: String,

    // Options below are not yet supported
    #[serde(rename = "Name")]
    pub name: String,
    #[serde(rename = "LogChat")]
    pub log_chat: bool,
    #[serde(rename = "Debug")]
    pub debug: bool,
    #[serde(rename = "AuthKey")]
    pub auth_key: Option<String>,
    #[serde(rename = "Private")]
    pub private: bool,
    #[serde(rename = "Description")]
    pub description: String,
    #[serde(rename = "ResourceFolder")]
    pub resource_folder: String,
}
