use super::{
    Backend,
    ServerBoundPluginEvent,
    PluginBoundPluginEvent,
    ScriptEvent,
    Argument
};

use std::sync::Arc;

use tokio::sync::mpsc::{Sender, Receiver};
use tokio::sync::oneshot;

use mlua::prelude::*;
use mlua::{UserData, UserDataMethods, Value, Function, Variadic};

#[derive(Clone)]
struct Context {
    tx: Arc<Sender<ServerBoundPluginEvent>>,
}

impl Context {
    fn new(tx: Arc<Sender<ServerBoundPluginEvent>>) -> Self {
        Self {
            tx,
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
            // TODO: Figure out how to handle these errors (?)
            let _ = me.tx.blocking_send(ServerBoundPluginEvent::RegisterEventHandler((event_name, handler_name)));
            Ok(())
        });

        methods.add_function("GetServerVersion", |_lua, ()| {
            Ok((1, 0, 0))
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

        // methods.add_function("GetPlayers", |lua, ()| {
        //     let me: Context = lua.globals().get("MP")?;
        //     let (tx, rx) = oneshot::channel();
        //     if let Err(e) = me.tx.blocking_send(ServerBoundPluginEvent::RequestPlayerCount(tx)) {
        //         error!("Failed to send packet: {:?}", e);
        //     }
        //     let message = rx.blocking_recv();
        //     if let Ok(message) = message {
        //         if let PluginBoundPluginEvent::PlayerCount(player_count) = message {
        //             Ok(player_count)
        //         } else {
        //             unreachable!() // This really should never be reachable
        //         }
        //     } else {
        //         todo!("Receiving a response from the server failed! How?")
        //     }
        // });
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

    fn call_event_handler(&mut self, event: ScriptEvent, args: Vec<Argument>) {
        let event_name = match event {
            ScriptEvent::OnPluginLoaded => "onPluginLoaded",
        };

        let func: LuaResult<Function> = self.lua.globals().get(event_name);
        if let Ok(func) = func {
            let mapped_args = args.into_iter().map(|arg| {
                match arg {
                    Argument::String(s) => if let Ok(lua_str) = self.lua.create_string(&s) { Some(Value::String(lua_str)) } else { None },
                    Argument::Boolean(b) => Some(Value::Boolean(b)),
                    Argument::Number(f) => Some(Value::Number(f as f64)),
                }
            }).filter(|v| v.is_some());
            if let Err(e) = func.call::<_, ()>(Variadic::from_iter(mapped_args)) {
                error!("[LUA] {}", e);
            }
        }
    }
}
