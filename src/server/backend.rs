use serde::de::DeserializeOwned;
use std::collections::HashMap;

static BACKEND_URL: &'static str = "backend.beammp.com";
static AUTH_URL: &'static str = "auth.beammp.com";

pub async fn authentication_request<R: DeserializeOwned>(
    target: &str,
    map: HashMap<String, String>,
) -> anyhow::Result<R> {
    let client = reqwest::Client::new();
    let resp = client
        .post(format!("https://{}/{}", AUTH_URL, target))
        .json(&map)
        .send()
        .await?;
    // panic!("json: {:?}", resp.text().await);
    Ok(resp.json().await?)
}
