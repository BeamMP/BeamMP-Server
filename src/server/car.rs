use glam::*;

use std::time::Instant;

#[derive(Default, Clone, Debug)]
pub struct Car {
    pub car_json: String,

    pub pos: DVec3,
    pub rot: DQuat,
    pub vel: DVec3,
    pub rvel: DVec3,
    pub tim: f64,
    pub ping: f64,
    pub last_pos_update: Option<Instant>,
}

impl Car {
    pub fn new(car_json: String) -> Self {
        Self {
            car_json: car_json,

            ..Default::default()
        }
    }

    pub fn raw_position(&self) -> DVec3 {
        self.pos
    }

    pub fn raw_rotation(&self) -> DQuat {
        self.rot
    }

    pub fn position(&self) -> DVec3 {
        self.pos + self.vel * self.last_pos_update.map(|t| t.elapsed().as_secs_f64()).unwrap_or(0.0)
    }

    pub fn rotation(&self) -> DQuat {
        let t = self.last_pos_update.map(|t| t.elapsed().as_secs_f64()).unwrap_or(0.0);
        self.rot + DQuat::from_euler(glam::EulerRot::YXZ, self.rvel.x * t, self.rvel.y * t, self.rvel.z * t)
    }
}
