use nalgebra::*;

use std::time::{Instant, Duration};

#[derive(Default, Clone, Debug)]
pub struct Car {
    pub car_json: String,

    pub pos: Vector3<f64>,
    pub rot: Quaternion<f64>,
    pub vel: Vector3<f64>,
    pub rvel: Vector3<f64>,
    pub tim: f64,
    pub ping: f64,
    pub last_pos_update: Option<Instant>,
}

impl Car {
    pub fn new(car_json: String) -> Self {
        Self {
            car_json,

            ..Default::default()
        }
    }

    pub fn pos(&self) -> Vector3<f64> {
        self.pos + self.vel * self.last_pos_update.map(|t| t.elapsed().as_secs_f64()).unwrap_or(0.0)
    }

    pub fn rotation(&self) -> Quaternion<f64> {
        let t = self.last_pos_update.map(|t| t.elapsed().as_secs_f64()).unwrap_or(0.0);
        self.rot + UnitQuaternion::from_euler_angles(self.rvel.x * t, self.rvel.y * t, self.rvel.z * t).quaternion()
    }
}
