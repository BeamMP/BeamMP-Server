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
        // Valid key format
        // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        // -8--------4----4----4----12---------
        if self.auth_key.is_none() {return false}
        let key = self.auth_key.clone().unwrap();
        let key_check: Vec<&str> = key.split("-").collect();
        if key_check.len() != 5 {return false}
        else if key_check[0].len() != 8 {return false}
        else if key_check[1].len() != 4 {return false}
        else if key_check[2].len() != 4 {return false}
        else if key_check[3].len() != 4 {return false}
        else if key_check[4].len() != 12 {return false}
        true
    }
}