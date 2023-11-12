use super::{Backend, ServerBoundPluginEvent};
use std::sync::Arc;
use tokio::sync::mpsc::Sender;
use mlua::prelude::*;
use mlua::{UserData, UserDataMethods};

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

impl UserData for Context {
    fn add_methods<'lua, M: UserDataMethods<'lua, Self>>(methods: &mut M) {
        methods.add_method("RegisterEventHandler", |_, me, (event_name, handler_name): (String, String)| {
            debug!("Event handler registered: {} (EVENT) = {} (LUA)", event_name, handler_name);
            // TODO: Figure out how to handle these errors (?)
            let _ = me.tx.blocking_send(ServerBoundPluginEvent::RegisterEventHandler((event_name, handler_name)));
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

        // let register_event_handler_fn = self.lua.create_function(|_lua, (name, handler_name): (String, String)| {
        //     tx.blocking_send(ServerBoundPluginEvent::RegisterEventHandler((name, handler_name)));
        //     Ok(())
        // })?;

        // let api = self.lua.create_table()?;
        let api = Context::new(tx);
        // api.set("", thing)?;

        let globals = self.lua.globals();
        globals.set("MP", api)?;
        globals.set("print", print_fn)?;

        Ok(())
    }
}
