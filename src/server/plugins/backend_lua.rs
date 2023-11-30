use super::{
    Backend,
    ServerBoundPluginEvent,
    PluginBoundPluginEvent,
    ScriptEvent,
    Argument
};

use std::sync::{Arc, Mutex};
use std::collections::HashMap;

use tokio::sync::mpsc::{Sender, Receiver};
use tokio::sync::oneshot;

use mlua::prelude::*;
use mlua::{UserData, UserDataMethods, Value, Function, Variadic};

#[derive(Clone)]
struct Context {
    tx: Arc<Sender<ServerBoundPluginEvent>>,

    handlers: Arc<Mutex<HashMap<String, String>>>,
}

impl Context {
    fn new(tx: Arc<Sender<ServerBoundPluginEvent>>) -> Self {
        Self {
            tx,

            handlers: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

impl<'lua> FromLua<'lua> for Context {
    fn from_lua(value: Value<'lua>, _: &'lua Lua) -> LuaResult<Self> {
        match value {
            Value::UserData(ud) => Ok(ud.borrow::<Self>()?.clone()),
            _ => unreachable!()
        }
    }
}

impl UserData for Context {
    fn add_methods<'lua, M: UserDataMethods<'lua, Self>>(methods: &mut M) {
        // Implement all the API functions here

        methods.add_function("RegisterEventHandler", |lua, (event_name, handler_name): (String, String)| {
            debug!("Event handler registered: {} (EVENT) = {} (LUA)", event_name, handler_name);
            let me: Context = lua.globals().get("MP")?;
            me.handlers.lock().expect("Lock is poisoned!").insert(event_name, handler_name);
            Ok(())
        });

        methods.add_function("GetServerVersion", |_lua, ()| {
            Ok((3, 3, 0))
        });

        methods.add_function("GetPlayerCount", |lua, ()| {
            let me: Context = lua.globals().get("MP")?;
            let (tx, rx) = oneshot::channel();
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPlayerCount(tx)) {
                error!("Failed to send packet: {:?}", e);
            }
            let message = rx.blocking_recv();
            if let Ok(message) = message {
                if let PluginBoundPluginEvent::PlayerCount(player_count) = message {
                    Ok(player_count)
                } else {
                    unreachable!() // This really should never be reachable
                }
            } else {
                todo!("Receiving a response from the server failed! How?")
            }
        });

        methods.add_function("GetPlayers", |lua, ()| {
            let me: Context = lua.globals().get("MP")?;
            let (tx, rx) = oneshot::channel();
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPlayers(tx)) {
                error!("Failed to send packet: {:?}", e);
            }
            let message = rx.blocking_recv();
            if let Ok(message) = message {
                if let PluginBoundPluginEvent::Players(players) = message {
                    let table = lua.create_table()?;
                    for (id, name) in players {
                        table.set(id, name)?;
                    }
                    Ok(table)
                } else {
                    unreachable!() // This really should never be reachable
                }
            } else {
                todo!("Receiving a response from the server failed! How?")
            }
        });

        methods.add_function("GetPlayerIdentifiers", |lua, (id,): (u8,)| {
            let me: Context = lua.globals().get("MP")?;
            let (tx, rx) = oneshot::channel();
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPlayerIdentifiers((id, tx))) {
                error!("Failed to send packet: {:?}", e);
            }
            let message = rx.blocking_recv();
            if let Ok(message) = message {
                if let PluginBoundPluginEvent::PlayerIdentifiers(identifiers) = message {
                    let table = lua.create_table()?;
                    table.set("id", identifiers.ip)?;
                    table.set("beammp_id", identifiers.beammp_id)?;
                    Ok(table)
                } else {
                    unreachable!() // This should really never be reachable
                }
            } else {
                todo!("Receiving a response from the server failed! How?")
            }
        });

        methods.add_function("GetPlayerName", |lua, (id,): (u8,)| {
            let me: Context = lua.globals().get("MP")?;
            // We can simply request all player names and pick the one we need
            let (tx, rx) = oneshot::channel();
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPlayers(tx)) {
                error!("Failed to send packet: {:?}", e);
            }
            let message = rx.blocking_recv();
            if let Ok(message) = message {
                if let PluginBoundPluginEvent::Players(players) = message {
                    let mut name = None;
                    'search: for (pid, pname) in players {
                        if pid == id {
                            name = Some(pname);
                            break 'search;
                        }
                    }
                    Ok(name)
                } else {
                    unreachable!() // This really should never be reachable
                }
            } else {
                todo!("Receiving a response from the server failed! How?")
            }
        });

        methods.add_function("GetPlayerVehicles", |lua, (id,): (u8,)| {
            let me: Context = lua.globals().get("MP")?;
            let (tx, rx) = oneshot::channel();
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPlayerVehicles((id, tx))) {
                error!("Failed to send packet: {:?}", e);
            }
            let message = rx.blocking_recv();
            if let Ok(message) = message {
                if let PluginBoundPluginEvent::PlayerVehicles(vehicles) = message {
                    let table = lua.create_table()?;
                    for (vid, veh_json) in vehicles {
                        table.set(vid, veh_json)?;
                    }
                    Ok(table)
                } else {
                    unreachable!() // This really should never be reachable
                }
            } else {
                todo!("Receiving a response from the server failed! How?")
            }
        });

        methods.add_function("GetPositionRaw", |lua, (pid, vid): (u8, u8)| {
            let me: Context = lua.globals().get("MP")?;
            let (tx, rx) = oneshot::channel();
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPositionRaw((pid, vid, tx))) {
                error!("Failed to send packet: {:?}", e);
            }
            let message = rx.blocking_recv();
            if let Ok(message) = message {
                if let PluginBoundPluginEvent::PositionRaw(pos_raw) = message {
                    let pos_table = lua.create_table()?;
                    pos_table.set("x", pos_raw.pos[0])?;
                    pos_table.set("y", pos_raw.pos[1])?;
                    pos_table.set("z", pos_raw.pos[2])?;

                    let table = lua.create_table()?;
                    table.set("pos", pos_table)?;
                    Ok(table)
                } else {
                    unreachable!() // This really should never be reachable
                }
            } else {
                todo!("Receiving a response from the server failed! How?")
            }
        });

        methods.add_function("SendChatMessage", |lua, (id, msg): (isize, String)| {
            let me: Context = lua.globals().get("MP")?;
            if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::SendChatMessage((id, msg))) {
                error!("Failed to send packet: {:?}", e);
            }
            Ok(())
        });
    }
}

pub struct BackendLua {
    lua: Lua,
}

impl BackendLua {
    pub fn new() -> Self {
        let lua = Lua::new();

        Self {
            lua,
        }
    }
}

impl Backend for BackendLua {
    fn load(&mut self, code: String) -> anyhow::Result<()> {
        self.lua.load(code).exec().map_err(|e| { error!("[LUA] {:?}", e); e })?;
        Ok(())
    }

    fn load_api(&mut self, tx: Arc<Sender<ServerBoundPluginEvent>>) -> anyhow::Result<()> {
        let print_fn = self.lua.create_function(|_lua, (msg,): (String,)| {
            info!("[LUA] {}", msg);
            Ok(())
        })?;

        let api = Context::new(tx);

        let globals = self.lua.globals();
        globals.set("MP", api)?;
        globals.set("print", print_fn)?;

        Ok(())
    }

    fn call_event_handler(&mut self, event: ScriptEvent, resp: Option<oneshot::Sender<Argument>>) {
        let (event_name, args) = match event {
            ScriptEvent::OnPluginLoaded => ("onInit", vec![]),
            ScriptEvent::OnShutdown => ("onShutdown", vec![]),

            ScriptEvent::OnPlayerAuthenticated { name, role, is_guest, identifiers } => ("onPlayerAuth", vec![Argument::String(name), Argument::String(role), Argument::Boolean(is_guest), Argument::Table(identifiers.to_map())]),
            ScriptEvent::OnPlayerConnecting { pid } => ("onPlayerConnecting", vec![Argument::Integer(pid as i64)]),
            ScriptEvent::OnPlayerJoining { pid } => ("onPlayerJoining", vec![Argument::Integer(pid as i64)]),

            ScriptEvent::OnPlayerDisconnect { pid, name } => ("onPlayerDisconnect", vec![Argument::Integer(pid as i64), Argument::String(name)]),

            ScriptEvent::OnVehicleSpawn { pid, vid, car_data } => ("onVehicleSpawn", vec![Argument::Integer(pid as i64), Argument::Integer(vid as i64), Argument::String(car_data)]),
            ScriptEvent::OnVehicleEdited { pid, vid, car_data } => ("onVehicleEdited", vec![Argument::Integer(pid as i64), Argument::Integer(vid as i64), Argument::String(car_data)]),
            ScriptEvent::OnVehicleDeleted { pid, vid } => ("onVehicleDeleted", vec![Argument::Integer(pid as i64), Argument::Integer(vid as i64)]),

            ScriptEvent::OnChatMessage { pid, name, message } => ("onChatMessage", vec![Argument::Integer(pid as i64), Argument::String(name), Argument::String(message)]),
        };

        let mut ret = Value::Number(-1f64);
        // TODO: Error handling
        {
            let ctx: Context = self.lua.globals().get("MP").expect("MP is missing!");
            let lock = ctx.handlers.lock().expect("Mutex is poisoned!");
            if let Some(handler_name) = lock.get(event_name) {
                let func: LuaResult<Function> = self.lua.globals().get(handler_name.clone());
                if let Ok(func) = func {
                    let mapped_args = args.into_iter().map(|arg| {
                        arg_to_value(&self.lua, arg)
                    }).filter(|v| v.is_some());
                    match func.call::<_, Value>(Variadic::from_iter(mapped_args)) {
                        Ok(res) => { trace!("fn ret: {:?}", res); ret = res; }
                        Err(e) => {
                            error!("[LUA] {}", e);
                            ret = Value::Number(-1f64);
                        },
                    }
                }
            }
        }

        debug!("sending result...");
        if let Some(resp) = resp {
            let arg = value_to_arg(ret);
            resp.send(arg).expect("Failed to send!");
        }
        debug!("call_event_handler done");
    }
}

fn value_to_arg(value: Value) -> Argument {
    match value {
        Value::Boolean(b) => Argument::Boolean(b),
        Value::Integer(i) => Argument::Integer(i),
        Value::Number(f) => Argument::Number(f as f32),
        Value::String(s) => Argument::String(s.to_string_lossy().to_string()),
        _ => Argument::Number(-1f32),
    }
}

fn arg_to_value(lua: &Lua, arg: Argument) -> Option<Value> {
    match arg {
        Argument::String(s) => if let Ok(lua_str) = lua.create_string(&s) { Some(Value::String(lua_str)) } else { None },
        Argument::Boolean(b) => Some(Value::Boolean(b)),
        Argument::Number(f) => Some(Value::Number(f as f64)),
        Argument::Integer(i) => Some(Value::Integer(i as i64)),
        Argument::Table(t) => {
            if let Ok(table) = lua.create_table() {
                for (key, value) in t {
                    if let Some(v) = arg_to_value(lua, value) {
                        if let Err(e) = table.set(key, v) {
                            error!("[LUA] Error occured trying to put data into table: {:?}", e);
                        }
                    }
                }
                Some(Value::Table(table))
            } else {
                None
            }
        }
    }
}
