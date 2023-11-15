use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{
    tcp::{OwnedReadHalf, OwnedWriteHalf},
    TcpStream,
};

pub async fn handle_http_get(socket: TcpStream) {
    let (read_half, write_half) = socket.into_split();
    todo!()
}

async fn read_get_request(socket: &mut OwnedReadHalf) {
    todo!()
}
