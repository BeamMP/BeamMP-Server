use serde::{Deserialize, Serialize};

#[derive(Debug, Clone)]
pub enum Packet {
    Raw(RawPacket),
    Notification(NotificationPacket),
}

impl Packet {
    pub fn get_header(&self) -> u32 {
        match self {
            Self::Raw(raw) => raw.header,
            Self::Notification(msg) => self.get_data().len() as u32,
        }
    }

    pub fn get_data(&self) -> &[u8] {
        match self {
            Self::Raw(raw) => &raw.data,
            Self::Notification(p) => p.0.as_bytes(),
        }
    }

    pub fn get_code(&self) -> Option<char> {
        match self {
            Self::Raw(raw) => raw.data.get(0).map(|c| *c as char),
            Self::Notification(_) => Some('J'),
        }
    }

    pub fn set_data(&mut self, data: Vec<u8>) {
        match self {
            Self::Raw(raw) => raw.data = data,
            Self::Notification(_) => todo!(),
        }
    }

    pub fn set_header(&mut self, header: u32) {
        match self {
            Self::Raw(raw) => raw.header = header,
            Self::Notification(_) => todo!(),
        }
    }

    pub fn data_as_string(&self) -> String {
        String::from_utf8_lossy(&self.get_data()).to_string()
    }
}

#[derive(Debug, Clone)]
pub struct NotificationPacket(String);

impl NotificationPacket {
    pub fn new<S: Into<String>>(msg: S) -> Self {
        Self(format!("J{}", msg.into()))
    }
}

/// Protocol:
/// Header: 4 bytes, contains data size
/// Data: Contains packet data
#[derive(Clone)]
pub struct RawPacket {
    pub header: u32,
    pub data: Vec<u8>,
}

impl RawPacket {
    pub fn from_code(code: char) -> Self {
        Self {
            header: 1,
            data: vec![code as u8],
        }
    }

    pub fn from_str(str_data: &str) -> Self {
        let data = str_data.as_bytes().to_vec();
        Self {
            header: data.len() as u32,
            data: data,
        }
    }

    pub fn data_as_string(&self) -> String {
        String::from_utf8_lossy(&self.data).to_string()
    }
}

impl std::fmt::Debug for RawPacket {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> Result<(), std::fmt::Error> {
        write!(
            f,
            "Header: `{:?}` - Bytes: `{:?}` - String: `{}`",
            self.header,
            self.data,
            self.data_as_string()
        )?;
        Ok(())
    }
}

#[derive(Default, Serialize, Deserialize)]
pub struct RespawnPacketData {
    pub pos: RespawnPacketDataPos,
    pub rot: RespawnPacketDataRot,
}

#[derive(Default, Serialize, Deserialize)]
pub struct RespawnPacketDataPos {
    pub x: f64,
    pub y: f64,
    pub z: f64,
}

#[derive(Default, Serialize, Deserialize)]
pub struct RespawnPacketDataRot {
    pub x: f64,
    pub y: f64,
    pub z: f64,
    pub w: f64,
}

#[derive(Default, Serialize, Deserialize)]
pub struct TransformPacket {
    pub rvel: [f64; 3],
    pub tim: f64,
    pub pos: [f64; 3],
    pub ping: f64,
    pub rot: [f64; 4],
    pub vel: [f64; 3],
}
